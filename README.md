# xv6-networking-stack
The main goal of this project is to Implement Networking Stack on xv6 till Ethernet Layer.
Networking is an important part of any OS implementation and our idea was to augment the current xv6 implementation with Networking Stack. Using the default E1000/N2K_PCI networking driver model (virtual driver provided by QEMU), we will build driver code and system calls to expose networking stack . We will showcase the output of this project, through an execution of ARP (Address Resolution Protocol) , which is basically a data frame over raw Ethernet Frame. More details are listed below. 

## High Level Tasks
- **QEMU Virtual Network Devices:** Either the E1000 default adapter provided by QEMU or the N2000 PCI network adapter.
- **QEMU network backend:** Either the TAP device backend or the SLiRP backend.
- **Device Driver:** Create the device driver for the chosen network interface adapter. The software would communicate with the device using port-mapped IO. The driver should implement DMA to copy data directly to and from the Physical memory to the network card. We'll create a simple implementation of the ring Buffers with all the memory for packet data allocated during initialization in the kernel.
- **Network Stack:** Only implemented till the Ethernet Layer with ARP for IPv4.
	- **MAC Layer:** Create and parse Ethernet frames.
	- **ARP Protocol:** Create and parse ARP packets.
- **ARP system call and client:** Expose the create_arp() call as a system call to test ARP through a user space client application.

# Team Members:
- Pradeep Kumar Beri <u1077774>
- Anmol Vatsa <u1082186>

# References:
- https://pdos.csail.mit.edu/6.828/2017/labs/lab6/
- https://www.cs.unh.edu/cnrg/people/gherrin/linux-net.html
