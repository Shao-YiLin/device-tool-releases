#include <malloc.h>
#include <vcruntime.h>
#include <io.h>

#include "mspeech_framework.h"

#include "include/lib_device_define.h"
#include "include/libspeech_device_manager.h"

static unsigned int FRAMEWORK_VRESION = 0x01000020;

static struct libspeech_device_manager *__get_handle(struct mspeech_framework *framework) {
	return framework->handle;
}

static void _init(struct mspeech_framework *framework,
	mspeech_framework_write_cb_fn write_cb,
	mspeech_framework_event_cb_fn event_cb,
	mspeech_framework_action_cb_fn action_cb,
	mspeech_framework_audio_cb_fn audio_cb,
	mspeech_framework_info_cb_fn info_cb, void *param) {

	if (NULL == framework->handle)
		framework->handle = malloc(sizeof(struct libspeech_device_manager));
	libspeech_device_manager_construct(framework->handle);

	if (NULL != framework->handle) {
		//	printf("libspeech_device_manager_construct");
		__get_handle(framework)->init(__get_handle(framework), write_cb, event_cb, action_cb, audio_cb, info_cb, param);
	}
}

static void _uninit(struct mspeech_framework *framework) {
	__get_handle(framework)->uninit(__get_handle(framework));

	if (NULL != framework->handle) {
		free(framework->handle);
		framework->handle = NULL;
	}
}

static void _bind_mac(struct mspeech_framework *framework, unsigned char *mac)
{
	if (NULL == mac) {
		if (NULL != framework->mac) {
			free(framework->mac);
			framework->mac = NULL;
		}
		return;
	}

	framework->mac = malloc(6);
	memcpy(framework->mac, mac, 6);
}

static void _set_protocol_version(struct mspeech_framework *framework, char version) {
	if (NULL != framework->handle)
		__get_handle(framework)->set_protocol_version(__get_handle(framework), version);
}

static void _set_dpi(struct mspeech_framework *framework, unsigned int dpi_level) {
	if (NULL != framework->handle)
		__get_handle(framework)->set_dpi(__get_handle(framework), dpi_level);
}

static void _get_dpi(struct mspeech_framework *framework) {

	if (NULL != framework->handle)
		__get_handle(framework)->get_dpi(__get_handle(framework));
}

static void _start_record(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->start_record(__get_handle(framework));
}

static void _stop_record(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->stop_record(__get_handle(framework));
}

static void _get_battery(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_battery(__get_handle(framework));
}

static void _get_device_version(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_device_version(__get_handle(framework));
}

static void _get_slave_device_version(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_slave_device_version(__get_handle(framework));
}

static void _get_device_macaddr(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_device_macaddr(__get_handle(framework));
}

static void _get_dongle_version(struct mspeech_framework *framework) {

	if (NULL != framework->handle)
		__get_handle(framework)->get_dongle_version(__get_handle(framework));
}

static void _get_dongle_macaddr(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_dongle_macaddr(__get_handle(framework));
}

static void _handshake(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->handshake(__get_handle(framework));
}

static void _get_device_productNo(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_device_productNo(__get_handle(framework));
}

static void _get_protocol_version(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_protocol_version(__get_handle(framework));
}

static void _set_record_channel_size(struct mspeech_framework *framework, int size) {
	if (NULL != framework->handle)
		__get_handle(framework)->set_record_channel_size(__get_handle(framework), size);
}

static void _set_sleep_time(struct mspeech_framework *framework, int time)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_sleep_time(__get_handle(framework), time);
}

static void _append_data(struct mspeech_framework *framework, unsigned char *data, unsigned int size) {
	if (NULL != framework->handle)
		__get_handle(framework)->append_data(__get_handle(framework), data, size);
}

static void _vibration_close(struct mspeech_framework *framework)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_vibration(__get_handle(framework), 0, 0, 0, 0);
}

static void _vibration_open(struct mspeech_framework *framework, unsigned char type, unsigned int duration)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_vibration(__get_handle(framework), 1, type, 0, duration);
}

static void _start_vibration(struct mspeech_framework *framework, unsigned int duration)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_vibration(__get_handle(framework), 2, 0, 0, 0);
}

static void _set_led_rgb(struct mspeech_framework *framework, unsigned char type, unsigned int duration, rgb_param *data, int size)
{
	if (size > 7)
		size = 7;
	if (NULL != framework->handle)
		__get_handle(framework)->set_led_rgb(__get_handle(framework), type, duration, (unsigned char *)data, sizeof(rgb_param) * size);
}

static void _set_custom_key(struct mspeech_framework *framework, unsigned char type, unsigned char key_index, unsigned char key_type, unsigned char *data, unsigned int size)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_custom_key(__get_handle(framework), type, key_index, key_type, data, size);
}

static void _start_sincheck(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->start_sincheck(__get_handle(framework));
}

static void _get_theme(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_theme(__get_handle(framework));
}

static unsigned int _get_version(struct mspeech_framework *framework) {
	return FRAMEWORK_VRESION;
}

/**
	 设置握手标记
	*/
static void _set_handshake_info(struct mspeech_framework *framework, unsigned int flag) {
	if (NULL != framework->handle)
		__get_handle(framework)->set_handshake_info(__get_handle(framework), flag);
}

static void _set_flip_page(struct mspeech_framework *framework, unsigned int value) {
	if (NULL != framework->handle)
		__get_handle(framework)->set_flip_page(__get_handle(framework), value);
}

static void _get_flip_page(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_flip_page(__get_handle(framework));
}

static void _get_subtitle_statu(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_subtitle_status(__get_handle(framework));
}

static void _get_work_mode(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_work_mode(__get_handle(framework));
}

static void _get_connection_type(struct mspeech_framework *framework)
{
	if (NULL != framework->handle)
		__get_handle(framework)->get_connection_type(__get_handle(framework));
}

static void _get_custom_key(struct mspeech_framework *framework, unsigned char index) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_custom_key(__get_handle(framework), index);
}

static void _start_ota(struct mspeech_framework *framework, unsigned char *session, unsigned char *firmware, unsigned int size) {
	if (NULL != framework->handle)
		__get_handle(framework)->start_ota(__get_handle(framework), session, firmware, size);
}

static void _end_ota(struct mspeech_framework *framework, unsigned int value) {
	if (NULL != framework->handle)
		__get_handle(framework)->end_ota(__get_handle(framework), value);
}

static void _set_polling_rate(struct mspeech_framework *framework, unsigned int value) {
	if (NULL != framework->handle)
		__get_handle(framework)->set_polling_rate(__get_handle(framework), value);
}

static void _get_polling_rate(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_polling_rate(__get_handle(framework));
}

static void _get_dongle_product_no(struct mspeech_framework *framework) {
	if (NULL != framework->handle)
		__get_handle(framework)->get_dongle_product_no(__get_handle(framework));
}

static void _get_led_intensity(struct mspeech_framework *framework, unsigned char index)
{
	if (NULL != framework->handle)
		__get_handle(framework)->get_led_intensity(__get_handle(framework),index);
}

static void _set_led_intensity(struct mspeech_framework *framework, unsigned char index, unsigned char value)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_led_intensity(__get_handle(framework),index, value);
}

static void _get_rgb_backlight_param(struct mspeech_framework *framework, unsigned char index)
{
	if (NULL != framework->handle)
		__get_handle(framework)->get_rgb_backlight_param(__get_handle(framework), index);
}

static void _set_rgb_backlight_param(struct mspeech_framework *framework, unsigned char index, unsigned char value)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_rgb_backlight_param(__get_handle(framework), index, value);
}

static void _get_laser_mode(struct mspeech_framework *framework)
{
	
	if (NULL != framework->handle)
		__get_handle(framework)->get_laser_mode(__get_handle(framework));
}

static void _set_laser_mode(struct mspeech_framework *framework, unsigned char value)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_laser_mode(__get_handle(framework), value);
}

static void _get_wheel_mode(struct mspeech_framework *framework)
{

	if (NULL != framework->handle)
		__get_handle(framework)->get_wheel_mode(__get_handle(framework));
}

static void _set_wheel_mode(struct mspeech_framework *framework, unsigned char value)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_wheel_mode(__get_handle(framework), value);
}

static void _set_os_type(struct mspeech_framework* framework, unsigned int type)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_os_type(__get_handle(framework), type);
}

static void _heart_beat(struct mspeech_framework *framework, unsigned char connect_status)
{
	if (NULL != framework->handle)
		__get_handle(framework)->heart_beat(__get_handle(framework), connect_status);
}

static void _player_play(struct mspeech_framework *framework, unsigned int tone_index)
{
	if (NULL != framework->handle)
		__get_handle(framework)->player_play(__get_handle(framework), tone_index);
}

static void _set_player_volume(struct mspeech_framework *framework, unsigned int volume)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_player_volume(__get_handle(framework), volume);
}

static void _get_player_volume(struct mspeech_framework *framework)
{
	if (NULL != framework->handle)
		__get_handle(framework)->get_player_volume(__get_handle(framework));
}

static void _set_time_heart_beat(struct mspeech_framework *framework, unsigned int time)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_time_heart_beat(__get_handle(framework), time);
}

static void _get_time_heart_beat(struct mspeech_framework *framework)
{
	if (NULL != framework->handle)
		__get_handle(framework)->get_time_heart_beat(__get_handle(framework));
}

static void _set_keyboard_custom_rgb(struct mspeech_framework *framework, unsigned int number, rgb_param_keyboard *data)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_keyboard_custom_rgb(__get_handle(framework), (unsigned char *)data, (number * sizeof(rgb_param_keyboard)));
}
static void _set_adaptive_modulation(struct mspeech_framework *framework, unsigned int value)
{
	if (NULL != framework->handle)
		__get_handle(framework)->set_adaptive_modulation(__get_handle(framework), value);
}

static void _get_adaptive_modulation(struct mspeech_framework *framework)
{
	if (NULL != framework->handle)
		__get_handle(framework)->get_adaptive_modulation(__get_handle(framework));
}

static void _get_ls_device_id(struct mspeech_framework *framework)
{
	if (NULL != framework->handle)
		__get_handle(framework)->get_ls_device_id(__get_handle(framework));
}

static void _get_ls_check_sum(struct mspeech_framework *framework, unsigned int time)
{
	if (NULL != framework->handle)
		__get_handle(framework)->get_ls_check_sum(__get_handle(framework), time);
}

void mspeech_framework_construct(struct mspeech_framework *framework) {
	if (framework) {
		framework->init = _init;
		framework->uninit = _uninit;
		framework->set_dpi = _set_dpi;
		framework->get_dpi = _get_dpi;
		framework->start_record = _start_record;
		framework->stop_record = _stop_record;
		framework->get_battery = _get_battery;
		framework->get_device_version = _get_device_version;
		framework->get_device_macaddr = _get_device_macaddr;
		framework->get_dongle_version = _get_dongle_version;
		framework->get_dongle_macaddr = _get_dongle_macaddr;
		framework->handshake = _handshake;
		framework->get_device_productNo = _get_device_productNo;
		framework->get_protocol_version = _get_protocol_version;
		framework->set_record_channel_size = _set_record_channel_size;
		framework->set_sleep_time = _set_sleep_time;
		framework->append_data = _append_data;
		framework->vibration_close = _vibration_close;
		framework->vibration_open = _vibration_open;
		framework->start_vibration = _start_vibration;
		framework->set_led_rgb = _set_led_rgb;
		framework->set_custom_key = _set_custom_key;
		framework->bind_mac = _bind_mac;
		framework->set_protocol_version = _set_protocol_version;
		framework->start_sincheck = _start_sincheck;
		framework->get_theme = _get_theme;
		framework->get_version = _get_version;
		framework->set_handshake_info = _set_handshake_info;
		framework->set_flip_page = _set_flip_page;
		framework->get_flip_page = _get_flip_page;
		framework->get_subtitle_status = _get_subtitle_statu;
		framework->get_work_mode = _get_work_mode;
		framework->get_custom_key = _get_custom_key;
		framework->get_connection_type = _get_connection_type;
		framework->start_ota = _start_ota;
		framework->end_ota = _end_ota;
		framework->set_polling_rate = _set_polling_rate;
		framework->get_polling_rate = _get_polling_rate;
		framework->get_dongle_product_no = _get_dongle_product_no;
		framework->get_led_intensity = _get_led_intensity;
		framework->set_led_intensity = _set_led_intensity;
		framework->get_rgb_backlight_param = _get_rgb_backlight_param;
		framework->set_rgb_backlight_param = _set_rgb_backlight_param;
		framework->get_laser_mode = _get_laser_mode;
		framework->set_laser_mode = _set_laser_mode;
		framework->get_slave_device_version = _get_slave_device_version;
		framework->get_wheel_mode = _get_wheel_mode;
		framework->set_wheel_mode = _set_wheel_mode;
		framework->set_os_type = _set_os_type;
		framework->heart_beat = _heart_beat;
		framework->player_play = _player_play;
		framework->set_player_volume = _set_player_volume;
		framework->get_player_volume = _get_player_volume;
		framework->set_time_heart_beat = _set_time_heart_beat;
		framework->get_time_heart_beat = _get_time_heart_beat;
		framework->set_keyboard_custom_rgb = _set_keyboard_custom_rgb;
		framework->set_adaptive_modulation = _set_adaptive_modulation;
		framework->get_adaptive_modulation = _get_adaptive_modulation;
		framework->get_ls_device_id = _get_ls_device_id;
		framework->get_ls_check_sum = _get_ls_check_sum;
		framework->mac = NULL;
		framework->handle = NULL;
	}
}

