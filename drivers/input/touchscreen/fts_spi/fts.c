// SPDX-License-Identifier: GPL-2.0-only
/*
 * Minimal SPI touchscreen driver for ST FTS controllers.
 */

#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>

#define FTS_DRV_NAME                     "fts"

#define FTS_CMD_SCAN_MODE                0xA0
#define FTS_SCAN_MODE_ACTIVE             0x00
#define FTS_ACTIVE_MULTI_TOUCH           0x01

#define FTS_FIFO_CMD_READALL             0x87
#define FTS_FIFO_EVENT_SIZE              8
#define FTS_FIFO_DEPTH                   32
#define FTS_FIFO_EVENTS_REMAINING_MASK   0x1f

#define FTS_EVT_ID_NOEVENT               0x00
#define FTS_EVT_ID_CONTROLLER_READY      0x03
#define FTS_EVT_ID_ENTER_POINT           0x13
#define FTS_EVT_ID_MOTION_POINT          0x23
#define FTS_EVT_ID_LEAVE_POINT           0x33

#define FTS_MAX_SLOTS                    10

struct fts_ts {
struct device *dev;
struct spi_device *spi;
struct input_dev *input;
struct touchscreen_properties prop;
struct mutex lock;

struct regulator *vdd;
struct regulator *avdd;
struct gpio_desc *reset_gpio;

u8 *fifo_buf;
DECLARE_BITMAP(active_slots, FTS_MAX_SLOTS);

u32 max_x;
u32 max_y;
bool super_resolution;
bool suspended;
};

static int fts_spi_write_cmd(struct fts_ts *ts, const u8 *cmd, size_t len)
{
return spi_write(ts->spi, cmd, len);
}

static int fts_spi_read_fifo(struct fts_ts *ts, u8 *out, unsigned int events)
{
struct spi_transfer xfers[2] = { };
u8 cmd = FTS_FIFO_CMD_READALL;
size_t data_len;
int error;

if (!events || events > FTS_FIFO_DEPTH)
return -EINVAL;

data_len = events * FTS_FIFO_EVENT_SIZE;
if (data_len + 1 > FTS_FIFO_DEPTH * FTS_FIFO_EVENT_SIZE + 1)
return -EINVAL;

ts->fifo_buf[0] = 0;
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

static int fts_start_scan(struct fts_ts *ts)
{
u8 cmd[3] = {
FTS_CMD_SCAN_MODE,
FTS_SCAN_MODE_ACTIVE,
FTS_ACTIVE_MULTI_TOUCH,
};

return fts_spi_write_cmd(ts, cmd, sizeof(cmd));
}

static int fts_stop_scan(struct fts_ts *ts)
{
u8 cmd[3] = {
FTS_CMD_SCAN_MODE,
FTS_SCAN_MODE_ACTIVE,
0x00,
};

return fts_spi_write_cmd(ts, cmd, sizeof(cmd));
}

static void fts_release_all_slots(struct fts_ts *ts)
{
unsigned int slot;

for (slot = 0; slot < FTS_MAX_SLOTS; slot++) {
input_mt_slot(ts->input, slot);
input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
}

bitmap_zero(ts->active_slots, FTS_MAX_SLOTS);
input_report_key(ts->input, BTN_TOUCH, 0);
input_report_key(ts->input, BTN_TOOL_FINGER, 0);
}

static void fts_report_down_event(struct fts_ts *ts, const u8 *event)
{
unsigned int slot = event[1] >> 4;
int x, y;
int major, minor;
int angle;

if (slot >= FTS_MAX_SLOTS)
return;

if (ts->super_resolution) {
x = (event[3] << 8) | event[2];
y = (event[5] << 8) | event[4];
} else {
x = ((event[3] & 0x0f) << 8) | event[2];
y = (event[4] << 4) | ((event[3] & 0xf0) >> 4);
}

x = clamp_val(x, 0, ts->max_x - 1);
y = clamp_val(y, 0, ts->max_y - 1);

major = ((event[0] & 0x0c) << 2) | ((event[6] & 0xf0) >> 4);
minor = ((event[7] & 0xc0) >> 2) | (event[6] & 0x0f);
angle = (s8)event[5];

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

static void fts_report_up_event(struct fts_ts *ts, const u8 *event)
{
unsigned int slot = event[1] >> 4;

if (slot >= FTS_MAX_SLOTS)
return;

input_mt_slot(ts->input, slot);
input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
__clear_bit(slot, ts->active_slots);

if (bitmap_empty(ts->active_slots, FTS_MAX_SLOTS)) {
input_report_key(ts->input, BTN_TOUCH, 0);
input_report_key(ts->input, BTN_TOOL_FINGER, 0);
}
}

static irqreturn_t fts_irq_thread(int irq, void *data)
{
struct fts_ts *ts = data;
u8 events[FTS_FIFO_DEPTH * FTS_FIFO_EVENT_SIZE] = { 0 };
unsigned int count;
unsigned int total_events = 1;
int error;

mutex_lock(&ts->lock);
if (ts->suspended)
goto out;

error = fts_spi_read_fifo(ts, events, 1);
if (error)
goto out;

total_events += events[7] & FTS_FIFO_EVENTS_REMAINING_MASK;
total_events = min_t(unsigned int, total_events, FTS_FIFO_DEPTH);

if (total_events > 1) {
error = fts_spi_read_fifo(ts, events + FTS_FIFO_EVENT_SIZE,
 total_events - 1);
if (error)
goto out;
}

for (count = 0; count < total_events; count++) {
const u8 *event = &events[count * FTS_FIFO_EVENT_SIZE];

switch (event[0]) {
case FTS_EVT_ID_NOEVENT:
count = total_events;
break;
case FTS_EVT_ID_ENTER_POINT:
case FTS_EVT_ID_MOTION_POINT:
fts_report_down_event(ts, event);
break;
case FTS_EVT_ID_LEAVE_POINT:
fts_report_up_event(ts, event);
break;
case FTS_EVT_ID_CONTROLLER_READY:
fts_start_scan(ts);
break;
default:
break;
}
}

input_mt_sync_frame(ts->input);
input_sync(ts->input);

out:
mutex_unlock(&ts->lock);
return IRQ_HANDLED;
}

static int fts_power_on(struct fts_ts *ts)
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

static void fts_power_off(struct fts_ts *ts)
{
if (ts->avdd)
regulator_disable(ts->avdd);
if (ts->vdd)
regulator_disable(ts->vdd);
}

static void fts_hw_reset(struct fts_ts *ts)
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

static int fts_parse_dt(struct fts_ts *ts)
{
struct device_node *np = ts->dev->of_node;
u32 value;
int gpio;

if (of_property_read_u32(np, "fts,x-max", &ts->max_x))
ts->max_x = 1080;
if (of_property_read_u32(np, "fts,y-max", &ts->max_y))
ts->max_y = 2400;

if (!of_property_read_u32(np, "fts,support-super-resolution", &value))
ts->super_resolution = !!value;
else
ts->super_resolution = true;

gpio = of_get_named_gpio(np, "fts,reset-gpio", 0);
if (gpio_is_valid(gpio)) {
ts->reset_gpio = devm_gpiod_get_from_of_node(
ts->dev, np, "fts,reset-gpio", 0, GPIOD_OUT_HIGH,
"fts-reset");
if (IS_ERR(ts->reset_gpio))
return PTR_ERR(ts->reset_gpio);
}

return 0;
}

static int fts_input_init(struct fts_ts *ts)
{
int error;

ts->input = devm_input_allocate_device(ts->dev);
if (!ts->input)
return -ENOMEM;

ts->input->name = "ST FTS Touchscreen";
ts->input->id.bustype = BUS_SPI;

input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, ts->max_x - 1, 0,
     0);
input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, ts->max_y - 1, 0,
     0);
input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
input_set_abs_params(ts->input, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
input_set_abs_params(ts->input, ABS_MT_ORIENTATION, -127, 127, 0, 0);

touchscreen_parse_properties(ts->input, true, &ts->prop);

error = input_mt_init_slots(ts->input, FTS_MAX_SLOTS,
   INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
if (error)
return error;

__set_bit(EV_KEY, ts->input->evbit);
__set_bit(BTN_TOUCH, ts->input->keybit);
__set_bit(BTN_TOOL_FINGER, ts->input->keybit);

return input_register_device(ts->input);
}

static int fts_probe(struct spi_device *spi)
{
struct fts_ts *ts;
unsigned long irq_flags;
int error;

ts = devm_kzalloc(&spi->dev, sizeof(*ts), GFP_KERNEL);
if (!ts)
return -ENOMEM;

ts->dev = &spi->dev;
ts->spi = spi;
mutex_init(&ts->lock);
spi_set_drvdata(spi, ts);

error = fts_parse_dt(ts);
if (error)
return error;

ts->vdd = devm_regulator_get_optional(ts->dev, "vdd");
if (IS_ERR(ts->vdd)) {
if (PTR_ERR(ts->vdd) == -ENODEV)
ts->vdd = NULL;
else
return PTR_ERR(ts->vdd);
}

ts->avdd = devm_regulator_get_optional(ts->dev, "avdd");
if (IS_ERR(ts->avdd)) {
if (PTR_ERR(ts->avdd) == -ENODEV)
ts->avdd = NULL;
else
return PTR_ERR(ts->avdd);
}

ts->fifo_buf = devm_kmalloc(ts->dev,
   FTS_FIFO_DEPTH * FTS_FIFO_EVENT_SIZE + 1,
   GFP_KERNEL);
if (!ts->fifo_buf)
return -ENOMEM;

error = fts_power_on(ts);
if (error)
return error;

fts_hw_reset(ts);

error = fts_input_init(ts);
if (error)
goto err_power_off;

if (spi->irq <= 0) {
error = -EINVAL;
goto err_power_off;
}

irq_flags = irq_get_trigger_type(spi->irq);
if (!irq_flags)
irq_flags = IRQF_TRIGGER_FALLING;
irq_flags |= IRQF_ONESHOT;

error = devm_request_threaded_irq(ts->dev, spi->irq, NULL, fts_irq_thread,
  irq_flags, FTS_DRV_NAME, ts);
if (error)
goto err_power_off;

error = fts_start_scan(ts);
if (error)
goto err_power_off;

device_init_wakeup(ts->dev, true);
return 0;

err_power_off:
fts_power_off(ts);
return error;
}

static void fts_remove(struct spi_device *spi)
{
struct fts_ts *ts = spi_get_drvdata(spi);

mutex_lock(&ts->lock);
ts->suspended = true;
fts_stop_scan(ts);
fts_release_all_slots(ts);
input_mt_sync_frame(ts->input);
input_sync(ts->input);
mutex_unlock(&ts->lock);

fts_power_off(ts);
}

static int __maybe_unused fts_suspend(struct device *dev)
{
struct fts_ts *ts = dev_get_drvdata(dev);

mutex_lock(&ts->lock);
ts->suspended = true;
fts_stop_scan(ts);
fts_release_all_slots(ts);
input_mt_sync_frame(ts->input);
input_sync(ts->input);
mutex_unlock(&ts->lock);

if (device_may_wakeup(dev))
enable_irq_wake(ts->spi->irq);

return 0;
}

static int __maybe_unused fts_resume(struct device *dev)
{
struct fts_ts *ts = dev_get_drvdata(dev);

if (device_may_wakeup(dev))
disable_irq_wake(ts->spi->irq);

mutex_lock(&ts->lock);
fts_hw_reset(ts);
fts_start_scan(ts);
ts->suspended = false;
mutex_unlock(&ts->lock);

return 0;
}

static const struct dev_pm_ops fts_pm_ops = {
SET_SYSTEM_SLEEP_PM_OPS(fts_suspend, fts_resume)
};

static const struct of_device_id fts_of_match[] = {
{ .compatible = "st,spi" },
{ }
};
MODULE_DEVICE_TABLE(of, fts_of_match);

static const struct spi_device_id fts_spi_id[] = {
{ "spi", 0 },
{ }
};
MODULE_DEVICE_TABLE(spi, fts_spi_id);

static struct spi_driver fts_spi_driver = {
.driver = {
.name = FTS_DRV_NAME,
.of_match_table = fts_of_match,
.pm = &fts_pm_ops,
},
.probe = fts_probe,
.remove = fts_remove,
.id_table = fts_spi_id,
};
module_spi_driver(fts_spi_driver);

MODULE_DESCRIPTION("Minimal ST FTS SPI touchscreen driver");
MODULE_AUTHOR("STMicroelectronics / Linux community");
MODULE_LICENSE("GPL");
