// SPDX-License-Identifier: GPL-2.0-only
/*
 * ST FTS V521 SPI touchscreen driver (core touch reporting only).
 */

#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>

#define ST_FTS_V521_DRIVER_NAME            "st-fts-v521"

#define ST_FTS_CMD_SCAN_MODE               0xA0
#define ST_FTS_CMD_SYSTEM                  0xA4
#define ST_FTS_SCAN_MODE_ACTIVE            0x00
#define ST_FTS_ACTIVE_MULTI_TOUCH          0x01
#define ST_FTS_SYS_CMD_SPECIAL             0x00
#define ST_FTS_SPECIAL_FIFO_FLUSH          0x01

#define ST_FTS_FIFO_CMD_READALL            0x87
#define ST_FTS_FIFO_EVENT_SIZE             8
#define ST_FTS_FIFO_DEPTH                  32
#define ST_FTS_FIFO_EVENTS_MASK            0x1f

#define ST_FTS_EVT_NOEVENT                 0x00
#define ST_FTS_EVT_CONTROLLER_READY        0x03
#define ST_FTS_EVT_ENTER_POINT             0x13
#define ST_FTS_EVT_MOTION_POINT            0x23
#define ST_FTS_EVT_LEAVE_POINT             0x33

#define ST_FTS_READY_TIMEOUT_MS            120
#define ST_FTS_RESET_RETRY_COUNT           3

#define ST_FTS_MAX_SLOTS                   10
#define ST_FTS_SLOT_SHIFT                  4
#define ST_FTS_X_HIGH_MASK                 0x0f
#define ST_FTS_Y_LOW_MASK                  0xf0
#define ST_FTS_MAJOR_TOP_MASK              0x0c
#define ST_FTS_MAJOR_LOW_MASK              0xf0
#define ST_FTS_MINOR_TOP_MASK              0xc0
#define ST_FTS_MINOR_LOW_MASK              0x0f

struct st_fts_v521 {
struct device *dev;
struct spi_device *spi;
struct input_dev *input;
struct touchscreen_properties prop;
struct mutex lock;

struct regulator *vdd;
struct regulator *avdd;
struct gpio_desc *reset_gpio;

u8 *fifo_buf;
DECLARE_BITMAP(active_slots, ST_FTS_MAX_SLOTS);

u32 max_x;
u32 max_y;
bool super_resolution;
bool suspended;
};

static void st_fts_reset(struct st_fts_v521 *ts);

static int st_fts_spi_write(struct st_fts_v521 *ts, const u8 *buf, size_t len)
{
return spi_write(ts->spi, buf, len);
}

static int st_fts_spi_read_fifo(struct st_fts_v521 *ts, u8 *out,
unsigned int events)
{
struct spi_transfer xfers[2] = { };
u8 cmd = ST_FTS_FIFO_CMD_READALL;
size_t data_len;
int error;

if (!events || events > ST_FTS_FIFO_DEPTH)
return -EINVAL;

data_len = events * ST_FTS_FIFO_EVENT_SIZE;

xfers[0].tx_buf = &cmd;
xfers[0].len = sizeof(cmd);
xfers[1].rx_buf = ts->fifo_buf;
xfers[1].len = data_len + 1;

error = spi_sync_transfer(ts->spi, xfers, ARRAY_SIZE(xfers));
if (error)
return error;

memcpy(out, ts->fifo_buf + 1, data_len);
return 0;
}

static int st_fts_start_scan(struct st_fts_v521 *ts)
{
u8 cmd[] = {
ST_FTS_CMD_SCAN_MODE,
ST_FTS_SCAN_MODE_ACTIVE,
ST_FTS_ACTIVE_MULTI_TOUCH,
};

return st_fts_spi_write(ts, cmd, sizeof(cmd));
}

static int st_fts_stop_scan(struct st_fts_v521 *ts)
{
u8 cmd[] = {
ST_FTS_CMD_SCAN_MODE,
ST_FTS_SCAN_MODE_ACTIVE,
0x00,
};

	return st_fts_spi_write(ts, cmd, sizeof(cmd));
}

static int st_fts_flush_fifo(struct st_fts_v521 *ts)
{
	u8 cmd[] = {
		ST_FTS_CMD_SYSTEM,
		ST_FTS_SYS_CMD_SPECIAL,
		ST_FTS_SPECIAL_FIFO_FLUSH,
	};

	return st_fts_spi_write(ts, cmd, sizeof(cmd));
}

static int st_fts_wait_for_ready(struct st_fts_v521 *ts)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(ST_FTS_READY_TIMEOUT_MS);
	u8 event[ST_FTS_FIFO_EVENT_SIZE] = { 0 };
	int error;

	do {
		error = st_fts_spi_read_fifo(ts, event, 1);
		if (error) {
			usleep_range(3000, 5000);
			continue;
		}

		if (event[0] == ST_FTS_EVT_CONTROLLER_READY)
			return 0;

		if (event[0] != ST_FTS_EVT_NOEVENT)
			dev_dbg(ts->dev, "during-reset event: %*ph\n",
				ST_FTS_FIFO_EVENT_SIZE, event);

		usleep_range(3000, 5000);
	} while (time_before(jiffies, timeout));

	return -ETIMEDOUT;
}

static int st_fts_hw_init_sequence(struct st_fts_v521 *ts)
{
	int attempt;
	int error;

	for (attempt = 1; attempt <= ST_FTS_RESET_RETRY_COUNT; attempt++) {
		st_fts_reset(ts);

		error = st_fts_wait_for_ready(ts);
		if (error) {
			dev_warn(ts->dev, "ready wait failed on attempt %d: %d\n",
				 attempt, error);
			continue;
		}

		error = st_fts_flush_fifo(ts);
		if (error) {
			dev_warn(ts->dev, "fifo flush failed on attempt %d: %d\n",
				 attempt, error);
			continue;
		}

		error = st_fts_start_scan(ts);
		if (!error)
			return 0;

		dev_warn(ts->dev, "start scan failed on attempt %d: %d\n",
			 attempt, error);
	}

	return error;
}

static void st_fts_release_all_slots(struct st_fts_v521 *ts)
{
unsigned int slot;

for (slot = 0; slot < ST_FTS_MAX_SLOTS; slot++) {
input_mt_slot(ts->input, slot);
input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
}

bitmap_zero(ts->active_slots, ST_FTS_MAX_SLOTS);
input_report_key(ts->input, BTN_TOUCH, 0);
input_report_key(ts->input, BTN_TOOL_FINGER, 0);
}

static void st_fts_report_down(struct st_fts_v521 *ts, const u8 *event)
{
unsigned int slot = event[1] >> ST_FTS_SLOT_SHIFT;
int x, y;
int major, minor;
int angle;

if (slot >= ST_FTS_MAX_SLOTS)
return;

if (ts->super_resolution) {
x = (event[3] << 8) | event[2];
y = (event[5] << 8) | event[4];
angle = 0;
} else {
x = ((event[3] & ST_FTS_X_HIGH_MASK) << 8) | event[2];
y = (event[4] << ST_FTS_SLOT_SHIFT) |
    ((event[3] & ST_FTS_Y_LOW_MASK) >> ST_FTS_SLOT_SHIFT);
angle = (s8)event[5];
}

x = clamp_val(x, 0, ts->max_x - 1);
y = clamp_val(y, 0, ts->max_y - 1);

major = ((event[0] & ST_FTS_MAJOR_TOP_MASK) << 2) |
((event[6] & ST_FTS_MAJOR_LOW_MASK) >> ST_FTS_SLOT_SHIFT);
minor = ((event[7] & ST_FTS_MINOR_TOP_MASK) >> 2) |
(event[6] & ST_FTS_MINOR_LOW_MASK);

input_mt_slot(ts->input, slot);
input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
touchscreen_report_pos(ts->input, &ts->prop, x, y, true);
input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, major * 16);
input_report_abs(ts->input, ABS_MT_TOUCH_MINOR, minor * 16);
input_report_abs(ts->input, ABS_MT_ORIENTATION, angle);
input_report_key(ts->input, BTN_TOUCH, 1);
input_report_key(ts->input, BTN_TOOL_FINGER, 1);
__set_bit(slot, ts->active_slots);
}

static void st_fts_report_up(struct st_fts_v521 *ts, const u8 *event)
{
unsigned int slot = event[1] >> ST_FTS_SLOT_SHIFT;

if (slot >= ST_FTS_MAX_SLOTS)
return;

input_mt_slot(ts->input, slot);
input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
__clear_bit(slot, ts->active_slots);

if (bitmap_empty(ts->active_slots, ST_FTS_MAX_SLOTS)) {
input_report_key(ts->input, BTN_TOUCH, 0);
input_report_key(ts->input, BTN_TOOL_FINGER, 0);
}
}

static irqreturn_t st_fts_irq_thread(int irq, void *data)
{
	struct st_fts_v521 *ts = data;
	u8 events[ST_FTS_FIFO_DEPTH * ST_FTS_FIFO_EVENT_SIZE] = { 0 };
unsigned int total_events = 1;
unsigned int idx;
int error;

mutex_lock(&ts->lock);

if (ts->suspended)
goto out;

	error = st_fts_spi_read_fifo(ts, events, 1);
	if (error) {
		dev_err_ratelimited(ts->dev, "failed to read first FIFO event: %d\n",
				    error);
		goto out;
	}

total_events += events[7] & ST_FTS_FIFO_EVENTS_MASK;
total_events = min_t(unsigned int, total_events, ST_FTS_FIFO_DEPTH);

if (total_events > 1) {
		error = st_fts_spi_read_fifo(ts, events + ST_FTS_FIFO_EVENT_SIZE,
					     total_events - 1);
		if (error) {
			dev_err_ratelimited(ts->dev,
					    "failed to read remaining FIFO events: %d\n",
					    error);
			goto out;
		}
	}

for (idx = 0; idx < total_events; idx++) {
const u8 *event = &events[idx * ST_FTS_FIFO_EVENT_SIZE];

switch (event[0]) {
case ST_FTS_EVT_NOEVENT:
idx = total_events;
break;
case ST_FTS_EVT_ENTER_POINT:
case ST_FTS_EVT_MOTION_POINT:
st_fts_report_down(ts, event);
break;
case ST_FTS_EVT_LEAVE_POINT:
st_fts_report_up(ts, event);
break;
		case ST_FTS_EVT_CONTROLLER_READY:
			error = st_fts_start_scan(ts);
			if (error) {
				dev_warn_ratelimited(ts->dev,
						     "controller-ready restart failed: %d\n",
						     error);
				goto out;
			}
			break;
		default:
			dev_dbg(ts->dev, "unhandled event: %*ph\n",
				ST_FTS_FIFO_EVENT_SIZE, event);
			break;
		}
	}

input_mt_sync_frame(ts->input);
input_sync(ts->input);

out:
mutex_unlock(&ts->lock);
return IRQ_HANDLED;
}

static int st_fts_power_on(struct st_fts_v521 *ts)
{
int error;

if (ts->vdd) {
error = regulator_enable(ts->vdd);
if (error)
return error;
}

if (ts->avdd) {
error = regulator_enable(ts->avdd);
if (error)
goto disable_vdd;
}

return 0;

disable_vdd:
if (ts->vdd)
regulator_disable(ts->vdd);
return error;
}

static void st_fts_power_off(struct st_fts_v521 *ts)
{
if (ts->avdd)
regulator_disable(ts->avdd);
if (ts->vdd)
regulator_disable(ts->vdd);
}

static void st_fts_reset(struct st_fts_v521 *ts)
{
if (!ts->reset_gpio) {
msleep(30);
return;
}

gpiod_set_value_cansleep(ts->reset_gpio, 0);
usleep_range(10000, 12000);
gpiod_set_value_cansleep(ts->reset_gpio, 1);
usleep_range(30000, 35000);
}

static int st_fts_parse_properties(struct st_fts_v521 *ts)
{
struct device_node *node = ts->dev->of_node;
u32 value;

	ts->max_x = 14400;
	ts->max_y = 32000;
	ts->super_resolution = false;

of_property_read_u32(node, "touchscreen-size-x", &ts->max_x);
of_property_read_u32(node, "touchscreen-size-y", &ts->max_y);

if (!of_property_read_u32(node, "fts,x-max", &value))
ts->max_x = value;
if (!of_property_read_u32(node, "fts,y-max", &value))
ts->max_y = value;

	if (!of_property_read_u32(node, "fts,support-super-resolution", &value))
		ts->super_resolution = !!value;

ts->reset_gpio = devm_gpiod_get_optional(ts->dev, "reset",
 GPIOD_OUT_HIGH);
if (IS_ERR(ts->reset_gpio))
return PTR_ERR(ts->reset_gpio);

return 0;
}

static int st_fts_init_input(struct st_fts_v521 *ts)
{
int error;

ts->input = devm_input_allocate_device(ts->dev);
if (!ts->input)
return -ENOMEM;

ts->input->name = "ST FTS V521 Touchscreen";
ts->input->id.bustype = BUS_SPI;

input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, ts->max_x - 1, 0,
     0);
input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, ts->max_y - 1, 0,
     0);
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, 4080, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MINOR, 0, 4080, 0, 0);
input_set_abs_params(ts->input, ABS_MT_ORIENTATION, -127, 127, 0, 0);

touchscreen_parse_properties(ts->input, true, &ts->prop);

error = input_mt_init_slots(ts->input, ST_FTS_MAX_SLOTS,
   INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
if (error)
return error;

__set_bit(EV_KEY, ts->input->evbit);
__set_bit(BTN_TOUCH, ts->input->keybit);
__set_bit(BTN_TOOL_FINGER, ts->input->keybit);

return input_register_device(ts->input);
}

static int st_fts_probe(struct spi_device *spi)
{
struct st_fts_v521 *ts;
unsigned long irq_flags;
int error;

ts = devm_kzalloc(&spi->dev, sizeof(*ts), GFP_KERNEL);
if (!ts)
return -ENOMEM;

	ts->dev = &spi->dev;
	if (!spi->bits_per_word)
		spi->bits_per_word = 8;
	error = spi_setup(spi);
	if (error)
		return dev_err_probe(&spi->dev, error, "failed to setup SPI\n");

	ts->spi = spi;
	mutex_init(&ts->lock);
	spi_set_drvdata(spi, ts);

	error = st_fts_parse_properties(ts);
	if (error)
		return dev_err_probe(&spi->dev, error, "failed to parse DT properties\n");

	ts->vdd = devm_regulator_get_optional(ts->dev, "vdd");
	if (IS_ERR(ts->vdd)) {
		if (PTR_ERR(ts->vdd) == -ENODEV)
			ts->vdd = NULL;
		else
			return dev_err_probe(ts->dev, PTR_ERR(ts->vdd),
					     "failed to get vdd regulator\n");
	}

	ts->avdd = devm_regulator_get_optional(ts->dev, "avdd");
	if (IS_ERR(ts->avdd)) {
		if (PTR_ERR(ts->avdd) == -ENODEV)
			ts->avdd = NULL;
		else
			return dev_err_probe(ts->dev, PTR_ERR(ts->avdd),
					     "failed to get avdd regulator\n");
	}

ts->fifo_buf = devm_kmalloc(ts->dev,
   ST_FTS_FIFO_DEPTH * ST_FTS_FIFO_EVENT_SIZE + 1,
   GFP_KERNEL);
if (!ts->fifo_buf)
return -ENOMEM;

	error = st_fts_power_on(ts);
	if (error)
		return dev_err_probe(ts->dev, error, "failed to enable power\n");

error = st_fts_init_input(ts);
if (error)
goto err_power_off;

	if (spi->irq <= 0) {
		error = -EINVAL;
		dev_err(ts->dev, "invalid IRQ %d\n", spi->irq);
		goto err_power_off;
	}

irq_flags = irq_get_trigger_type(spi->irq);
if (!irq_flags)
irq_flags = IRQF_TRIGGER_FALLING;
irq_flags |= IRQF_ONESHOT;

	error = devm_request_threaded_irq(ts->dev, spi->irq, NULL,
					  st_fts_irq_thread, irq_flags,
					  ST_FTS_V521_DRIVER_NAME, ts);
	if (error) {
		dev_err(ts->dev, "failed to request IRQ %d: %d\n", spi->irq, error);
		goto err_power_off;
	}

	error = st_fts_hw_init_sequence(ts);
	if (error) {
		dev_err(ts->dev, "failed to initialize controller: %d\n", error);
		goto err_power_off;
	}

	device_init_wakeup(ts->dev, true);
	dev_dbg(ts->dev, "initialized irq=%d max=(%u,%u) superres=%u\n",
		spi->irq, ts->max_x, ts->max_y, ts->super_resolution);
	return 0;

err_power_off:
st_fts_power_off(ts);
return error;
}

static void st_fts_remove(struct spi_device *spi)
{
struct st_fts_v521 *ts = spi_get_drvdata(spi);

mutex_lock(&ts->lock);
ts->suspended = true;
st_fts_stop_scan(ts);
st_fts_release_all_slots(ts);
input_mt_sync_frame(ts->input);
input_sync(ts->input);
mutex_unlock(&ts->lock);

st_fts_power_off(ts);
}

static int __maybe_unused st_fts_suspend(struct device *dev)
{
struct st_fts_v521 *ts = dev_get_drvdata(dev);
int error;

mutex_lock(&ts->lock);
ts->suspended = true;
st_fts_stop_scan(ts);
st_fts_release_all_slots(ts);
input_mt_sync_frame(ts->input);
input_sync(ts->input);
mutex_unlock(&ts->lock);

if (device_may_wakeup(dev)) {
error = enable_irq_wake(ts->spi->irq);
if (error)
dev_warn(dev, "failed to enable wake IRQ: %d\n", error);
}

return 0;
}

static int __maybe_unused st_fts_resume(struct device *dev)
{
struct st_fts_v521 *ts = dev_get_drvdata(dev);
int error;

if (device_may_wakeup(dev)) {
error = disable_irq_wake(ts->spi->irq);
if (error)
dev_warn(dev, "failed to disable wake IRQ: %d\n", error);
}

	mutex_lock(&ts->lock);
	ts->suspended = false;
	error = st_fts_hw_init_sequence(ts);
	if (error)
	dev_warn(dev, "failed to restart controller: %d\n", error);
	mutex_unlock(&ts->lock);

return 0;
}

static const struct dev_pm_ops st_fts_pm_ops = {
SET_SYSTEM_SLEEP_PM_OPS(st_fts_suspend, st_fts_resume)
};

static const struct of_device_id st_fts_of_match[] = {
{ .compatible = "st,fts-v521-spi" },
{ }
};
MODULE_DEVICE_TABLE(of, st_fts_of_match);

static const struct spi_device_id st_fts_id[] = {
	{ "st,fts-v521-spi", 0 },
	{ "fts-v521-spi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, st_fts_id);

static struct spi_driver st_fts_driver = {
.driver = {
.name = ST_FTS_V521_DRIVER_NAME,
.of_match_table = st_fts_of_match,
.pm = &st_fts_pm_ops,
},
.id_table = st_fts_id,
.probe = st_fts_probe,
.remove = st_fts_remove,
};
module_spi_driver(st_fts_driver);

MODULE_DESCRIPTION("ST FTS V521 SPI touchscreen driver");
MODULE_LICENSE("GPL");
