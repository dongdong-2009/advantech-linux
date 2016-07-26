/*
 * Platform suspend/resume/poweroff quirk example
 *
 * Copyright (C) 2015 Dialog Semiconductor Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/mfd/da9063/registers.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>

static struct i2c_client *da9063_client;

static int dms_ba16_suspend_pm_cb(struct notifier_block *nb,
				unsigned long action, void *ptr)
{
	switch (action) {
	case PM_SUSPEND_PREPARE:
	case PM_HIBERNATION_PREPARE:
		/*
		 * E.G. ADJUST PMIC SEQUENCER FOR SUSPEND
		 * E.G. MANIPULATE CONTROL LINE USAGE
		 * i2c_smbus_write_byte_data(dms_ba16_client, <REGISTER>, <VALUE>);
		 */



		break;
	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
		/*
		 * RESTORE PMIC SEQUENCER / CONTROL LINES
		 * i2c_smbus_write_byte_data(dms_ba16_client, <REGISTER>, <VALUE>);
		 */
                i2c_smbus_write_byte_data(da9063_client,0xA4,0x70); // VBCORE1_A(1.42V)
                i2c_smbus_write_byte_data(da9063_client,0xA3,0x70); // VBCORE2_A(1.42V)
                i2c_smbus_write_byte_data(da9063_client,0xA5,0x61); // VBPRO_A(1.5V)
                i2c_smbus_write_byte_data(da9063_client,0xA6,0x32); // VBMEM_A(1.8V)
                i2c_smbus_write_byte_data(da9063_client,0xA7,0x32); // VBIO_A(1.8V)
                i2c_smbus_write_byte_data(da9063_client,0xA8,0x7d); // VBPERI_A(3.3V)




		break;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static void dms_ba16_poweroff_quirk(void)
{
	/*
	 * Do nothing - this must be assigned as pm_power_off callback, or
	 * otherwise /kernel/reboot.c : SYSCALL_DEFINE4(reboot, ... reduces
	 * LINUX_REBOOT_CMD_POWER_OFF to LINUX_REBOOT_CMD_HALT
	 * and so the pm_power_off_prepare callback would never be used!
	 *
	 * This callback is now apparently too late in the power off process
	 * for dms_ba16 I2C work, as it caused a stack dump with the message:
	 *      WARNING: CPU: 0 PID: 50 at kernel/workqueue.c:1958
	 *      process_one_work+0x3bc/0x424()
	 */
}
static void da9063_poweroff_prepare_quirk(void)
{
	/* E.G. SET PMIC MODE AND POWER OFF */
	u8 val = 0;


	printk(KERN_ALERT "Poweroff DA9063\n");


	i2c_smbus_write_byte_data(da9063_client, DA9063_REG_CONTROL_F,DA9063_SHUTDOWN);


	while (1);

	return;
}

static int dms_ba16_reboot_notify(struct notifier_block *nb,
				unsigned long action, void *data)
{
	switch (action) {
	case SYS_POWER_OFF:
		break;
	case SYS_HALT:
	case SYS_RESTART:
		/*
		 * E.G. RESTORE PMIC SEQUENCER
		 * E.G. MODIFY GPIO TO RESET SLAVE DEVICE
		 * i2c_smbus_write_byte_data(dms_ba16_client, <REGISTER>, <VALUE>);
		 */
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block dms_ba16_reboot_nb = {
	.notifier_call = dms_ba16_reboot_notify
};

static int platform_i2c_bus_notify(struct notifier_block *nb,
				   unsigned long action, void *data)
{
	struct device *dev = data;
	struct i2c_client *client;

	if ((action != BUS_NOTIFY_ADD_DEVICE) ||
	    (dev->type == &i2c_adapter_type))
		 return 0;

	client = to_i2c_client(dev);

	if ((client->addr == 0x58 && !strcmp(client->name, "da9063"))) {
		da9063_client = client;

		/*
		 * E.G. SET IRQ MASKS
		 * i2c_smbus_write_byte_data(dms_ba16_client, <REGISTER>, <VALUE>);
		 */

		/*
		 * Register PM notifier for suspend/resume switchovers
		 * of control
		 */

                 i2c_smbus_write_byte_data(da9063_client,0x0B,0xF7); /*IRQ_MASK_B*/
                 i2c_smbus_write_byte_data(da9063_client,0x25,0x9);  /*BPERI_CONT keep BPERI_B voltage when supsend*/
                 i2c_smbus_write_byte_data(da9063_client,0x24,0x1);  /*BIO_CONT let BIO_B off when supsend*/



		pm_notifier(dms_ba16_suspend_pm_cb, 0);

		/* Register reboot notifier */
		register_reboot_notifier(&dms_ba16_reboot_nb);

		/* Establish poweroff callback */
		printk(KERN_INFO "Installing DA9063 poweroff control\n");
		pm_power_off_prepare = da9063_poweroff_prepare_quirk;
		pm_power_off = dms_ba16_poweroff_quirk;

		/* Get rid of this notification */
		bus_unregister_notifier(&i2c_bus_type, nb);
	}

	return 0;
}

static struct notifier_block platform_i2c_bus_nb = {
	.notifier_call = platform_i2c_bus_notify
};

static int __init platform_quirk(void)
{
	da9063_client = NULL;
	bus_register_notifier(&i2c_bus_type, &platform_i2c_bus_nb);
	return 0;
}

arch_initcall(platform_quirk);

