/*
 * epinic.h: Interface for the epinic device driver.
 *
 * Copiright (c) 1997 Epigram, Inc.
 *
 * $Id: epindd.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef	_NTDDK_

/* Stuff from ntddk.h for user programs */

#if !defined(_NDIS_)
/* Define the I/O bus interface types. */
typedef enum _INTERFACE_TYPE {
	InterfaceTypeUndefined = -1,
	Internal,
	Isa,
	Eisa,
	MicroChannel,
	TurboChannel,
	PCIBus,
	VMEBus,
	NuBus,
	PCMCIABus,
	CBus,
	MPIBus,
	MPSABus,
	ProcessorInternal,
	InternalPowerBus,
	PNPISABus,
	MaximumInterfaceType
}INTERFACE_TYPE, *PINTERFACE_TYPE;
#endif // endif

/* Define types of bus information. */
typedef enum _BUS_DATA_TYPE {
	ConfigurationSpaceUndefined = -1,
	Cmos,
	EisaConfiguration,
	Pos,
	CbusConfiguration,
	PCIConfiguration,
	VMEConfiguration,
	NuBusConfiguration,
	PCMCIAConfiguration,
	MPIConfiguration,
	MPSAConfiguration,
	PNPISAConfiguration,
	MaximumBusDataType
} BUS_DATA_TYPE, *PBUS_DATA_TYPE;

/* Physical address. */
typedef LARGE_INTEGER PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;

#endif	/* _NTDDK_ */

/* Assign bases for our constants in Microsoft's "Reserved for customers" spaces */

#define	FILE_DEVICE_EPINDD	0x0a230		/* 2595 << 4 */

#define	EPINDD_IOCTL_INDEX	0x0a23		/* 2595 */

/* Our ioctls: */

#define	IOCTL_EPINDD_GET_PCI_CONFIG		\
	CTL_CODE (FILE_DEVICE_EPINDD, EPINDD_IOCTL_INDEX, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_SET_PCI_CONFIG		\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX +  1), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_MAP_SPACE			\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX +  2), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_UNMAP_SPACE		\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX +  3), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_CLAIM_PCI_CARD		\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX +  4), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_DISCLAIM_PCI_CARD		\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX +  5), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_SIZE_PCI_BARS		\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX +  6), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_MAP_COM_BUF		\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX +  7), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_UNMAP_COM_BUF		\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX +  8), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_IOREAD			\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX +  9), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_IOWRITE			\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX + 10), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_DBG			\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX + 31), METHOD_BUFFERED, FILE_ANY_ACCESS)
#define	IOCTL_EPINDD_SVCNAME		\
	CTL_CODE (FILE_DEVICE_EPINDD, (EPINDD_IOCTL_INDEX + 32), METHOD_BUFFERED, FILE_ANY_ACCESS)

/* Parameter structure for PCI config ioctl's */

typedef struct {
	int	bus;		/* Bus number */
	int	slot;		/* (Virtual) slot number */
	int	fun;		/* Function number for multifunction devices */
	int	off;		/* Offset into configuration space */
	int	count;		/* Byte count */
} pci_config_parms;

/* Parameter structure for the map/unmap ioctl's */

typedef struct
{
	INTERFACE_TYPE		it;		/* Isa, Eisa, etc.... */
	int			bus;		/* Bus number */
	PHYSICAL_ADDRESS	addr;		/* Bus-relative address */
	int			as;		/* 0 is memory, 1 is I/O */
	int			len;		/* Length of section to map */
} space_parms;

/* Size information */
typedef struct {
	int	bus,
		slot,
		fun,
		bar;
	unsigned long	size;
} bar_size_info;
