/*
 * @Author: xuhan
 * @Date: 2020-02-08 19:46:05
 * @LastEditors  : xuhan
 * @LastEditTime : 2020-02-09 22:09:02
 * @Description:
 */

#include <stdio.h>
#include <string.h>
#include "mblue_heap.h"

#include "libspeech_device_manager.h"
#include "lib_device_manager.h"

static struct lib_device_manager *__get_handle(void *param)
{
	return (struct lib_device_manager *)param;
}

static void _init(struct libspeech_device_manager *dm,
	libmouse_write_cb_fn write_cb,
	libmouse_event_cb_fn event_cb,
	libmouse_action_cb_fn action_cb,
	libmouse_audio_cb_fn audio_cb,
	libmouse_info_cb_fn info_cb, void *param)
{

	if (NULL == dm->handle)
		dm->handle = mblue_malloc(sizeof(struct lib_device_manager));
	lib_get_device_manager(dm->handle);
	__get_handle(dm->handle)->init(dm->handle, write_cb, event_cb, action_cb, audio_cb, info_cb, param);
}

static void _uninit(struct libspeech_device_manager *dm)
{
	if (dm->handle)
	{
		__get_handle(dm->handle)->uninit(dm->handle);
		mblue_free(dm->handle);
		dm->handle = 0;
	}
}

static void _set_dpi(struct libspeech_device_manager *dm, unsigned int dpi_level)
{
	__get_handle(dm->handle)->set_dpi(dm->handle, dpi_level);
}

static void _get_dpi(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_dpi(dm->handle);
}

static void _start_record(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->start_record(dm->handle);
}

static void _stop_record(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->stop_record(dm->handle);
}

static void _get_battery(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_battery(dm->handle);
}

static void _get_device_version(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_device_version(dm->handle);
}
static void _get_slave_device_version(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_slave_device_version(dm->handle);
}


static void _get_device_macaddr(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_device_macaddr(dm->handle);
}

static void _get_dongle_version(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_dongle_version(dm->handle);
}

static void _get_dongle_macaddr(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_dongle_macaddr(dm->handle);
}

static void _handshake(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->handshake(dm->handle);
}

static void _get_device_productNo(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_device_productNo(dm->handle);
}

static void _get_protocol_version(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_protocol_version(dm->handle);
}

static void _set_record_channel_size(struct libspeech_device_manager *dm, int size)
{
	__get_handle(dm->handle)->set_record_channel_size(dm->handle, size);
}

static void _set_sleep_time(struct libspeech_device_manager *dm, int time)
{
	__get_handle(dm->handle)->set_sleep_time(dm->handle, time);
}

//static void _start_authentication(struct libspeech_device_manager *dm)
//{
//    __get_handle(dm->handle)->start_authentication(dm->handle);
//}

static void _append_data(struct libspeech_device_manager *dm, unsigned char *data, unsigned int size) {
	__get_handle(dm->handle)->append_data(dm->handle, data, size);
}

static void _set_led_rgb(struct libspeech_device_manager *dm, unsigned char type, unsigned int duration, unsigned char *data, int size)
{
	__get_handle(dm->handle)->set_led_rgb(dm->handle, type, duration, data, size);
}

static void _set_vibration(struct libspeech_device_manager *dm, unsigned char status, unsigned char type, unsigned char level, unsigned int duration) {
	__get_handle(dm->handle)->set_vibration(dm->handle, status, type, level, duration);
}

static void _set_custom_key(struct libspeech_device_manager *dm, unsigned char type, unsigned char key_index, unsigned char key_type, unsigned char *data, unsigned int size) {
	__get_handle(dm->handle)->set_custom_key(dm->handle, type, key_index, key_type, data, size);
}

static void _set_protocol_version(struct libspeech_device_manager *dm, char version) {
	__get_handle(dm->handle)->known_protocol_type = version;
}

static void _start_sincheck(struct libspeech_device_manager *dm) {
	__get_handle(dm->handle)->start_sincheck(dm->handle);
}

static void _get_theme(struct libspeech_device_manager *dm) {
	__get_handle(dm->handle)->get_theme(dm->handle);
}

static void _set_handshake_info(struct libspeech_device_manager *dm, unsigned int flag) {
	__get_handle(dm->handle)->set_handshake_info(dm->handle, flag);
}

static void _set_flip_page(struct libspeech_device_manager *dm, unsigned int value) {
	__get_handle(dm->handle)->set_flip_page(dm->handle, value);
}

static void _get_flip_page(struct libspeech_device_manager *dm) {
	__get_handle(dm->handle)->get_flip_page(dm->handle);
}

static void _get_subtitle_status(struct libspeech_device_manager *dm) {
	__get_handle(dm->handle)->get_subtitle_satus(dm->handle);
}

static void _get_work_mode(struct libspeech_device_manager *dm) {
	__get_handle(dm->handle)->get_work_mode(dm->handle);
}

static void _get_connection_type(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_connection_type(dm->handle);
}

static void _get_custom_key(struct libspeech_device_manager *dm, unsigned char index)
{
	__get_handle(dm->handle)->get_custom_key(dm->handle, index);
}

static void _start_ota(struct libspeech_device_manager *dm, unsigned char *session, unsigned char *firmware, unsigned int size) {
	__get_handle(dm->handle)->start_ota(dm->handle, session, firmware, size);
}

static void _end_ota(struct libspeech_device_manager *dm, unsigned int value) {
	__get_handle(dm->handle)->end_ota(dm->handle, value);
}

static void _set_polling_rate(struct libspeech_device_manager *dm, unsigned int value)
{
	__get_handle(dm->handle)->set_polling_rate(dm->handle, value);
}

static void _get_polling_rate(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_polling_rate(dm->handle);
}

static void _get_dongle_product_no(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_dongle_product_no(dm->handle);
}

static void _get_led_intensity(struct libspeech_device_manager *dm, unsigned char index)
{
	__get_handle(dm->handle)->get_led_intensity(dm->handle,index);
}

static void _set_led_intensity(struct libspeech_device_manager *dm, unsigned char index, unsigned char value)
{
	__get_handle(dm->handle)->set_led_intensity(dm->handle, index,value);
}

static void _get_rgb_backlight_param(struct libspeech_device_manager *dm, unsigned char index)
{
	__get_handle(dm->handle)->get_rgb_backlight_param(dm->handle, index);
}

static void _set_rgb_backlight_param(struct libspeech_device_manager *dm, unsigned char index, unsigned char value)
{
	
	__get_handle(dm->handle)->set_rgb_backlight_param(dm->handle, index, value);
}

static void _get_laser_mode(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_laser_mode(dm->handle);
}

static void _set_laser_mode(struct libspeech_device_manager *dm, unsigned char value)
{
	__get_handle(dm->handle)->set_laser_mode(dm->handle,value);
}

static void _get_wheel_mode(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_wheel_mode(dm->handle);
}

static void _set_wheel_mode(struct libspeech_device_manager *dm, unsigned char value)
{
	__get_handle(dm->handle)->set_wheel_mode(dm->handle, value);
}

static void _set_os_type(struct libspeech_device_manager* dm, unsigned int type)
{
	__get_handle(dm->handle)->set_os_type(dm->handle, type);
}

static void _heart_beat(struct libspeech_device_manager *dm, unsigned char connect_status)
{
	__get_handle(dm->handle)->heart_beat(dm->handle, connect_status);
}

static void _player_play(struct libspeech_device_manager *dm, unsigned int tone_index)
{
	__get_handle(dm->handle)->player_play(dm->handle, tone_index);
}

static void _set_player_volume(struct libspeech_device_manager *dm, unsigned int volume)
{
	__get_handle(dm->handle)->set_player_volume(dm->handle, volume);
}

static void _get_player_volume(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_player_volume(dm->handle);
}

static void _set_time_heart_beat(struct libspeech_device_manager *dm, unsigned int time)
{
	__get_handle(dm->handle)->set_time_heart_beat(dm->handle, time);
}

static void _get_time_heart_beat(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_time_heart_beat(dm->handle);
}

static void _set_keyboard_custom_rgb(struct libspeech_device_manager *dm, unsigned char *data, unsigned int size)
{
	__get_handle(dm->handle)->set_keyboard_custom_rgb(dm->handle, data, size);
}
static void _set_adaptive_modulation(struct libspeech_device_manager *dm,  unsigned int value)
{
	__get_handle(dm->handle)->set_adaptive_modulation(dm->handle, value);
}

static void _get_adaptive_modulation(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_adaptive_modulation(dm->handle);
}

static void _get_ls_device_id(struct libspeech_device_manager *dm)
{
	__get_handle(dm->handle)->get_ls_device_id(dm->handle);
}

static void _get_ls_check_sum(struct libspeech_device_manager *dm, unsigned int time)
{
	__get_handle(dm->handle)->get_ls_check_sum(dm->handle, time);
}

static void __install_device_manager_construct(struct libspeech_device_manager *dm)
{
	if (!dm)
		return;

	dm->init = _init;
	dm->uninit = _uninit;
	dm->set_dpi = _set_dpi;
	dm->get_dpi = _get_dpi;
	dm->start_record = _start_record;
	dm->stop_record = _stop_record;
	dm->get_battery = _get_battery;
	dm->get_device_version = _get_device_version;
	dm->get_device_macaddr = _get_device_macaddr;
	dm->get_dongle_version = _get_dongle_version;
	dm->get_dongle_macaddr = _get_dongle_macaddr;
	dm->handshake = _handshake;
	dm->get_device_productNo = _get_device_productNo;
	dm->get_protocol_version = _get_protocol_version;
	dm->set_record_channel_size = _set_record_channel_size;
	dm->set_sleep_time = _set_sleep_time;
	//  dm->start_authentication = _start_authentication;
	dm->append_data = _append_data;
	dm->set_led_rgb = _set_led_rgb;
	dm->set_vibration = _set_vibration;
	dm->set_custom_key = _set_custom_key;
	dm->set_protocol_version = _set_protocol_version;
	dm->start_sincheck = _start_sincheck;
	dm->get_theme = _get_theme;
	dm->set_handshake_info = _set_handshake_info;
	dm->set_flip_page = _set_flip_page;
	dm->get_flip_page = _get_flip_page;
	dm->get_subtitle_status = _get_subtitle_status;
	dm->get_work_mode = _get_work_mode;
	dm->get_connection_type = _get_connection_type;
	dm->get_custom_key = _get_custom_key;
	dm->start_ota = _start_ota;
	dm->end_ota = _end_ota;
	dm->set_polling_rate = _set_polling_rate;
	dm->get_polling_rate = _get_polling_rate;
	dm->get_dongle_product_no = _get_dongle_product_no;
	dm->get_led_intensity = _get_led_intensity;
	dm->set_led_intensity = _set_led_intensity;
	dm->get_rgb_backlight_param = _get_rgb_backlight_param;
	dm->set_rgb_backlight_param = _set_rgb_backlight_param;
	dm->get_laser_mode = _get_laser_mode;
	dm->set_laser_mode = _set_laser_mode;
	dm->get_slave_device_version = _get_slave_device_version;
	dm->get_wheel_mode = _get_wheel_mode;
	dm->set_wheel_mode = _set_wheel_mode;
	dm->set_os_type = _set_os_type;
	dm->heart_beat = _heart_beat;
	dm->player_play = _player_play;
	dm->set_player_volume = _set_player_volume;
	dm->get_player_volume = _get_player_volume;
	dm->set_time_heart_beat = _set_time_heart_beat;
	dm->get_time_heart_beat = _get_time_heart_beat;
	dm->set_keyboard_custom_rgb = _set_keyboard_custom_rgb;
	dm->set_adaptive_modulation = _set_adaptive_modulation;
	dm->get_adaptive_modulation = _get_adaptive_modulation;
	dm->get_ls_device_id = _get_ls_device_id;
	dm->get_ls_check_sum = _get_ls_check_sum;
	dm->handle = NULL;
}

void libspeech_device_manager_construct(struct libspeech_device_manager *dm)
{
	if (!dm)
		return;

	__install_device_manager_construct(dm);
}