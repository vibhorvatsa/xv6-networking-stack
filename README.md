# xv6-networking-stack
A limited network stack implementation for xv6 OS.

## High Level Tasks
- **QEMU Virtual Network Devices:** Either the E1000 default adapter provided by QEMU or the N2000 PCI network adapter.
- **QEMU network backend:** Either the TAP device backend or the SLiRP backend.
- **Device Driver:** Create the device driver for the chosen network interface adapter. The software would communicate with the device using port-mapped IO. The driver should implement DMA to copy data directly to and from the Physical memory to the network card. We'll create a simple implementation of the ring Buffers with all the memory for packet data allocated during initialization in the kernel.
- **Network Stack:** Only implemented till the Ethernet Layer with ARP for IPv4.
	- **MAC Layer:** Create and parse Ethernet frames.
	- **ARP Protocol:** Create and parse ARP packets.
- **ARP system call and client:** Expose the create_arp() call as a system call to test ARP through a user space client application.
