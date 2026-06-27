#ifndef SMBUS_DRV_H
#define SMBUS_DRV_H

void smbus_drv_init(void);
void smbus_drv_timer_1ms(void);
void smbus_drv_background_task(void);

/*
    SMBALERT# transport service.

    These APIs only drive/release the side-band line and enable ARA transport.
    The upper-layer device protocol owns the fault/status policy and decides
    when release is legal.
*/
void smbus_drv_assert_alert(void);
void smbus_drv_release_alert(void);
unsigned char smbus_drv_is_alert_asserted(void);

#endif
