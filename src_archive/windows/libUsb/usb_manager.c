#include <stdio.h>
#include <string.h>
#include "usb_manager.h"
#include "include/commondefine.h"

static int is_my_device(struct device_info *dest, struct libusb_device_descriptor desc)
{
    if (desc.idVendor == dest->vendor_id && desc.idProduct == dest->product_id)
    {
        return 1;
    }
    return 0;
}

static int get_endpoit(struct usb_manager* um)
{
    int ret = 0;
    libusb_device* dev = um->dev;
    struct libusb_config_descriptor* config;
    um->in_endpoit = 0;
    um->out_endpoit = 0;
    ret = libusb_get_config_descriptor(dev, 0, &config);
    if (ret != LIBUSB_SUCCESS)
    {
        printf("get config error\r\n");
        return ret;
    }
        
    log_d("total Interfaces: %d \r\n ", (int)config->bNumInterfaces);
    const struct libusb_interface* inter;
    const struct libusb_interface_descriptor* interdesc;
    const struct libusb_endpoint_descriptor* epdesc;
    for (int i = 0; i < (int)config->bNumInterfaces; i++) {
        inter = &config->interface[i];
        for (int j = 0; j < inter->num_altsetting; j++) {
            interdesc = &inter->altsetting[j];
            if ((int)interdesc->bInterfaceNumber == 1)
            {
                for (int k = 0; k < (int)interdesc->bNumEndpoints; k++) {
                    epdesc = &interdesc->endpoint[k];
                    if (epdesc->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                    {
                        um->in_endpoit = epdesc->bEndpointAddress;
                        printf("in_endpoit:%x\r\n", um->in_endpoit);
                    }
                    else
                    {
                        um->out_endpoit = epdesc->bEndpointAddress;
                        printf("out_endpoit:%x\r\n", um->out_endpoit);
                    }
                }
            }
        }
    }
    libusb_free_config_descriptor(config);
    if (um->in_endpoit == 0 || um->out_endpoit == 0)
        return -1;
    return 0;
}

static libusb_device *__getCurrentDevice(struct usb_manager *um, struct device_info *device)
{
    int nRet = libusb_init(NULL);
    if (nRet < 0)
        return 0;

    libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, 3);
    ssize_t cnt = libusb_get_device_list(NULL, &um->devs);
    if (cnt < 0)
    {
        log_d("Get Device Error\n");
    }

	int i;
    for (i = 0; i < cnt; i++)
    {
        struct libusb_device_descriptor desc;
        int nRet = libusb_get_device_descriptor(um->devs[i], &desc);
        if (nRet < 0)
            return 0;
        if (is_my_device(device, desc))
        {
            uint8_t address = libusb_get_device_address(um->devs[i]);
            if (address == device->device_address)
            {
                return um->devs[i];
            }
        }
    }
    return NULL;
}

static int _init_new(struct usb_manager *um, struct device_info *device, USB_RECEIVE_CALLBACK callback, USB_REMOVE_CALLBACK remove_cb, void *param)
{
    if (NULL == um || NULL == device)
        return 0;


    if (um->lock_flag)
        return LIBUSB_ERROR_OTHER;

    um->cb_param = param;
    um->data_callback = callback;
	um->usb_remove_callback = remove_cb;
    um->dev = __getCurrentDevice(um, device);
    
    if (NULL != um->dev)
    {
        um->b_find_device_flag = true;
    }
    if (!um->b_find_device_flag)
        return LIBUSB_ERROR_OTHER;
    if (get_endpoit(um) < 0)
        return -13;
    um->dev_handler = NULL;
    int nRet = libusb_open(um->dev, &um->dev_handler);
    if (nRet != LIBUSB_SUCCESS)
    {
        log_d("libusb open fail \n");
        return nRet;
    }
    else
    {
        log_d("libusb open success \n");
    }

    if(NULL == um->dev_handler){
        log_d("libusb open fail \n");
        return LIBUSB_ERROR_OTHER;
    }

    if (1 == libusb_kernel_driver_active(um->dev_handler, USB_INTERFACE))
    {
        nRet = libusb_detach_kernel_driver(um->dev_handler, USB_INTERFACE);
        if (nRet != LIBUSB_SUCCESS)
        {
            log_d("libusb_detach_kernel_driver fail \n");
            um->uninit(um);
            return nRet;
        }
    }

    nRet = libusb_claim_interface(um->dev_handler, USB_INTERFACE);
    if (nRet != LIBUSB_SUCCESS)
    {
        log_d("libusb_claim_interface fail \n");
		um->uninit(um);
     //   um->dev_handler = NULL;
        return nRet;
    }

    um->lock_flag = 1;

    return 1;
}

static void _stop(struct usb_manager *um)
{

    if (NULL != um->dev_handler)
    {
        libusb_reset_device(um->dev_handler);
        libusb_release_interface(um->dev_handler, USB_INTERFACE);

        libusb_close(um->dev_handler); // 直接close，会出现异常卡死
        // log_d("libusb_close \n");
        um->dev_handler = NULL;
    }

    if (NULL != um->devs)
        libusb_free_device_list(um->devs, 1);
}

// void *_run(void *param)
// {
//     log_d("read service run... \n");

//     struct usb_manager *um = param;
//     um->bRun = true;
//     if (NULL != um->dev_handler)
//         libusb_reset_device(um->dev_handler);

//     int nRet = 0;
//     int actual = 0;

//     while (um->bRun)
//     {
//         //   usleep(100);
//         if (NULL != um->dev_handler)
//         {
//             memset(um->read_buffer, 0x0, sizeof(um->read_buffer));
//             nRet = libusb_interrupt_transfer(um->dev_handler, 3 | LIBUSB_ENDPOINT_IN, um->read_buffer, sizeof(um->read_buffer), &actual, 0);

//             if (!um->bRun)
//                 break;

//             log_d("read data %d \n", nRet);

//             if (nRet == 0)
//             {
//                 if (NULL != um->data_callback)
//                     um->data_callback(um->read_buffer, actual);
//             }
//         }
//     }
// }

static int _start_read(struct usb_manager *um)
{
    log_d("read service run... \n");

    if(!um)
         return 0;

    um->b_run = 1;
    if (NULL != um->dev_handler)
        libusb_reset_device(um->dev_handler);

    int nRet = 0;
    int actual = 0;

    while (um->b_run)
    {
        if (NULL != um->dev_handler)
        {
            memset(um->read_buffer, 0x0, sizeof(um->read_buffer));
            nRet = libusb_interrupt_transfer(um->dev_handler, um->in_endpoit, um->read_buffer, sizeof(um->read_buffer), &actual, 0);

            if (!um->b_run)
                break;

      //      log_d("usb read data %d, %d \n", actual, nRet);

            if (nRet == 0)
            {
                if (NULL != um->data_callback)
                    um->data_callback(um->cb_param, um->read_buffer, actual);
			}
			else {
				if (LIBUSB_ERROR_IO == nRet) {
					if (NULL != um->usb_remove_callback)
					{
						um->usb_remove_callback(um->cb_param);
					}
				}
				else
				{
					Sleep(1);
				}
			}
        }
    }
    return 1;
}

static int _stop_read(struct usb_manager *um) {
	if (um->b_run)
	{
		um->b_run = 0;
	}
	return 1;;
}

static int _write(struct usb_manager *um, uint8_t *data, uint32_t len)
{
    if (NULL == data || len <= 0)
        return 0;

    if (NULL == um->dev_handler)
        return 0;

    int actual = 0;
    int nRet = libusb_interrupt_transfer(um->dev_handler, um->out_endpoit, data, len, &actual, 0);
    if (nRet == 0)
    {
    //    log_d("usb write success \n");
    }
    else
    {
        log_d("usb write fail \n");
    }
    return 1;
}

static int _uninit(struct usb_manager *um)
{
    log_d("usb manager uninit \n");

    if (um->lock_flag != 1)
        return 1;

	um->stop_read(um);
	_stop(um);
    libusb_exit(NULL);

    um->lock_flag = 0;
	return 1;
}

static inline void __install_usb_manager_operation(struct usb_manager *manager)
{
    manager->init = _init_new;
    manager->start_read = _start_read;
	manager->stop_read = _stop_read;
    manager->write = _write;
    manager->uninit = _uninit;
    manager->dev_handler = NULL;
    manager->b_run = false;
    manager->b_find_device_flag = false;
	manager->data_callback = NULL;
	manager->usb_remove_callback = NULL;
	manager->cb_param = NULL;
    manager->lock_flag = 0;
}

void usb_construct(struct usb_manager *manager){
    if(NULL == manager)
        return;

    __install_usb_manager_operation(manager);
}

// struct usb_manager *usb_construct()
// {
//     struct usb_manager *manager = (struct usb_manager *)mblue_malloc(sizeof(struct usb_manager));
//     if (NULL == manager)
//         return NULL;

//     __install_usb_manager_operation(manager);
//     return manager;
// }

static int is_madao_device(libusb_device *device, unsigned short vendor_id, unsigned short product_id)
{
    struct libusb_device_descriptor desc;
    int nRet = libusb_get_device_descriptor(device, &desc);
    if (nRet < 0)
        return 0;

    if (desc.idVendor == vendor_id && desc.idProduct == product_id)
    {
        return 1;
    }

    return 0;
}

static void printdev(libusb_device *dev)
{
	struct libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(dev, &desc);
	if (r < 0) {
		log_d("failed to get device descriptor \n");
		return;
	}
	log_d("Number of possible configurations: %d \n",(int)desc.bNumConfigurations);
	log_d("Device Class: %d \n", (int)desc.bDeviceClass);
	log_d("VendorID: %04x \n", desc.idVendor);
	log_d("ProductID: %04x \n", desc.idProduct);
	struct libusb_config_descriptor *config;
	libusb_get_config_descriptor(dev, 0, &config);
	log_d("total Interfaces: %d \n " , (int)config->bNumInterfaces);
	const struct libusb_interface *inter;
	const struct libusb_interface_descriptor *interdesc;
	const struct libusb_endpoint_descriptor *epdesc;
	for (int i = 0; i < (int)config->bNumInterfaces; i++) {
		inter = &config->interface[i];
		log_d("------------------------------------------------------------- \n");
		log_d("Number of alternate settings: %d \n", inter->num_altsetting);
		for (int j = 0; j < inter->num_altsetting; j++) {
			interdesc = &inter->altsetting[j];
			log_d("-- Interface Number: %d \n" ,(int)interdesc->bInterfaceNumber);
			log_d("-- Number of endpoints: %d \n" ,(int)interdesc->bNumEndpoints);
			for (int k = 0; k < (int)interdesc->bNumEndpoints; k++) {
				epdesc = &interdesc->endpoint[k];
				log_d("---- Descriptor Type: %d \n",(int)epdesc->bDescriptorType);
				log_d("---- EP Address: %d \n" , (int)epdesc->bEndpointAddress);
			}
		}
	}
	log_d("\n\n\n");
	libusb_free_config_descriptor(config);
}

void scan_device(USB_SCAN_CALLBACK scan_callback, unsigned short vendor_id, unsigned short product_id)
{
    int nRet = libusb_init(NULL);
    if (nRet < 0)
        return;

    libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, 3);
    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0)
    {
        log_d("Get Device Error\n");
    }

    struct device_info devices[5] = {0};
    // struct device_info ele;

    int device_size = 0;
	int i;
    for (i = 0; i < cnt; i++)
    {
        if (is_madao_device(devs[i], vendor_id, product_id))
        {
            struct libusb_device_descriptor desc;
            int nRet = libusb_get_device_descriptor(devs[i], &desc);
            if (nRet == LIBUSB_SUCCESS)
            {
                devices[device_size].vendor_id = desc.idVendor;
                devices[device_size].product_id = desc.idProduct;
				devices[device_size].device_type = ((desc.bcdDevice >> 8) & 0x0f);
                devices[device_size].device_address = libusb_get_device_address(devs[i]);
				printf("serialNumber: %d\r\n", desc.iSerialNumber);
				
                device_size++;
				log_d("success \n");

				//libusb_device_handle *dev_handler = NULL;
				//int nRet = libusb_open(devs[i], &dev_handler);
				//if (nRet != LIBUSB_SUCCESS)
				//{
				//	log_d("libusb open fail \n");
				//}
				//else
				//{
				//	if (NULL == dev_handler) {
				//		log_d("libusb open fail \n");
				//	}
				//	else {
				//		u8 data[256] = { 0 };
				//	//	0x0409
				//		/*int nByte = libusb_get_string_descriptor(dev_handler, desc.iSerialNumber, (uint16_t)0x0409, serialNumber, sizeof(serialNumber));*/

				//		libusb_set_auto_detach_kernel_driver(dev_handler, 1);
				//		libusb_claim_interface(dev_handler, 1);

				//		if (desc.iSerialNumber) {
				//			int nByte = libusb_get_string_descriptor_ascii(dev_handler, desc.iSerialNumber, data, sizeof(data));
				//			printf("stringSerialSize: %d\r\n", nByte);
				//			printf("stringSerial: %s\r\n", data);
				//		}

				//		if (desc.iManufacturer) {
				//			int nByte = libusb_get_string_descriptor_ascii(dev_handler, desc.iManufacturer, data, sizeof(data));
				//			printf("stringManufacturer: %d\r\n", nByte);
				//			printf("stringManufacturer: %s\r\n", data);
				//		}

				//		if (desc.iProduct) {
				//			int nByte = libusb_get_string_descriptor_ascii(dev_handler, desc.iProduct, data, sizeof(data));
				//			printf("stringProduct: %d\r\n", nByte);
				//			printf("stringProduct: %s\r\n", data);
				//		}
				//		
				//	}
				//}

				//libusb_close(dev_handler);
	
				// printdev(devs[i]);
            //    log_d("%04x, %04x, %d \n", desc.idVendor, desc.idProduct, devices[device_size].device_address);
            }
        }
    }

    libusb_free_device_list(devs, 1);
    libusb_exit(NULL);

	if (NULL != scan_callback)
	{
		scan_callback(devices, device_size);
	}
}