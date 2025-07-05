#include "pci_ids.h"

const char* pci_lookup_vendor(uint16_t vendor_id) {
    for (const pci_vendor* vendor = pci_vendors; vendor->id != 0xFFFF; vendor++) {
        if (vendor->id == vendor_id) {
            return vendor->name;
        }
    }
    return "Unknown Vendor";
}

const char* pci_lookup_device(uint16_t vendor_id, uint16_t device_id) {
    for (const pci_device* device = pci_devices; 
         device->vendor_id != 0xFFFF || device->device_id != 0xFFFF; 
         device++) {
        if (device->vendor_id == vendor_id && device->device_id == device_id) {
            return device->name;
        }
    }
    return "Unknown Device";
}
