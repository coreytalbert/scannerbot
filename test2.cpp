#include <rtl-sdr.h>
#include <iostream>
using std::cout;

typedef int sdr_device;

rtlsdr_dev_t *device = nullptr;


int main() {
    cout << "\nSDR count: " << rtlsdr_get_device_count();

    const char *dev_name = rtlsdr_get_device_name(0);

    cout << "\nDevice name: " << dev_name;

    char manufact[256], product[256], serial[256];
    rtlsdr_get_device_usb_strings(0, manufact, product, serial);
    // device = rtlsdr_open(&device, 0);
}