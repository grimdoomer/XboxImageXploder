/*
	XboxImageXploder - Utility for modifying xbox executables.
	
	XboxExecutable.h - Types and functions for parsing and modifying xbox executable files.

	Author - Grimdoomer
*/

#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <map>

// ---------------------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------------------

#define XBE_IMAGE_HEADER_MAGIC			'HEBX'

#define XBE_IMAGE_HEADER_MIN_SIZE		0x170

#define XBE_IMAGE_SIGNATURE_LENGTH				256
#define XBE_IMAGE_SYMMETRICAL_KEY_LENGTH		16
#define XBE_IMAGE_DIGEST_LENGTH					20

#define XBE_IMAGE_FLAGS_MOUNT_UTILITY_DRIVE		1

#define XBE_IMAGE_ENTRYPOINT_XOR_DEBUG			0x94859D4B
#define XBE_IMAGE_ENTRYPOINT_XOR_RETAIL			0xA8FC57AB

#define XBE_IMAGE_THUNK_ADDRESS_XOR_DEBUG		0xEFB1F152
#define XBE_IMAGE_THUNK_ADDRESS_XOR_RETAIL		0x5B6D40B6

#define XBE_HEADER_OFFSET_OF(header, addr)		(addr - (header)->BaseAddress)

#define XBE_HEADER_ADDRESS_OF(header, ptr)		(((DWORD)((char*)(ptr) - (char*)header)) + (header)->BaseAddress)

#define ALIGN_TO(addr, align)		((size_t)(addr) + (((size_t)(addr) % align) == 0 ? 0 : align - ((size_t)(addr) % align)))

struct XBE_IMAGE_HEADER
{
	/* 0x00 */ DWORD		Magic;
	/* 0x04 */ BYTE			Signature[XBE_IMAGE_SIGNATURE_LENGTH];
	/* 0x104 */ DWORD		BaseAddress;
	/* 0x108 */ DWORD		SizeOfHeaders;
	/* 0x10C */ DWORD		SizeOfImage;
	/* 0x110 */ DWORD		SizeOfImageHeader;
	/* 0x114 */ DWORD		CreationTimestamp;
	/* 0x118 */ DWORD		CertificateAddress;
	/* 0x11C */ DWORD		NumberOfSections;
	/* 0x120 */ DWORD		SectionHeadersAddress;
	/* 0x124 */ DWORD		ImageFlags;
	/* 0x128 */ DWORD		EntryPoint;
	/* 0x12C */ DWORD		TLSAddress;
	/* 0x130 */ DWORD		PEStackCommit;
	/* 0x134 */ DWORD		PEHeapReserve;
	/* 0x138 */ DWORD		PEHeapCommit;
	/* 0x13C */ DWORD		PEBaseAddress;
	/* 0x140 */ DWORD		PESizeOfImage;
	/* 0x144 */ DWORD		PEChecksum;
	/* 0x148 */ DWORD		PETimestamp;
	/* 0x14C */ DWORD		FullFileNameAddress;
	/* 0x150 */ DWORD		FileNameAddress;
	/* 0x154 */ DWORD		UnicodeFileNameAddress;
	/* 0x158 */ DWORD		KernelImageThunkAddress;
	/* 0x15C */ DWORD		ImportTableAddress;
	/* 0x160 */ DWORD		NumberOfLibraryVersions;
	/* 0x164 */ DWORD		LibraryVersionsAddress;
	/* 0x168 */ DWORD		KernelLibraryVersionAddress;
	/* 0x16C */ DWORD		XAPILibraryVersionAddress;
	/* 0x170 */ DWORD		LogoBitmapAddress;
	/* 0x174 */ DWORD		LogoBitmapSize;
	/* 0x178 */ DWORD		LibraryFeaturesAddress;
	/* 0x17C */ DWORD		NumberOfLibraryFeatures;
	/* 0x180 */ DWORD		CodeViewDebugInfoAddress;
};

#define XBE_IMAGE_CERT_TITLE_NAME_LENGTH		40

#define XBE_IMAGE_CERTIFICATE_MIN_SIZE			0x1D0

struct XBE_IMAGE_CERTIFICATE
{
	/* 0x00 */ DWORD		Size;
	/* 0x04 */ DWORD		CreationTimestmap;
	/* 0x08 */ DWORD		TitleID;
	/* 0x0C */ WCHAR		TitleName[XBE_IMAGE_CERT_TITLE_NAME_LENGTH];
	/* 0x5C */ DWORD		AlternateTitleIDs[16];
	/* 0x9C */ DWORD		MediaFlags;
	/* 0xA0 */ DWORD		GameRegion;
	/* 0xA4 */ DWORD		GameRatings;
	/* 0xA8 */ DWORD		DiskNumber;
	/* 0xAC */ DWORD		Version;
	/* 0xB0 */ CHAR			LANKey[XBE_IMAGE_SYMMETRICAL_KEY_LENGTH];
	/* 0xC0 */ CHAR			SignatureKey[XBE_IMAGE_SYMMETRICAL_KEY_LENGTH];
	/* 0xD0 */ CHAR			AlternateSignatureKeys[16][XBE_IMAGE_SYMMETRICAL_KEY_LENGTH];
	/* 0x1D0 */ DWORD		OriginalSizeOfCertificate;
	/* 0x1D4 */ DWORD		OnlineServiceName;
	/* 0x1D8 */ DWORD		RuntimeSecurityFlags;
	/* 0x1DC */ CHAR		UnknownKey[XBE_IMAGE_SYMMETRICAL_KEY_LENGTH];
};

#define XBE_SECTION_FLAGS_WRITABLE				0x00000001
#define XBE_SECTION_FLAGS_PRELOAD				0x00000002
#define XBE_SECTION_FLAGS_EXECUTABLE			0x00000004
#define XBE_SECTION_FLAGS_INSERTED_FILE			0x00000008
#define XBE_SECTION_FLAGS_HEAD_PAGE_READ_ONLY	0x00000010
#define XBE_SECTION_FLAGS_TAIL_PAGE_READ_ONLY	0x00000020

struct XBE_IMAGE_SECTION_HEADER
{
	/* 0x00 */ DWORD		SectionFlags;
	/* 0x04 */ DWORD		VirtualAddress;
	/* 0x08 */ DWORD		VirtualSize;
	/* 0x0C */ DWORD		RawAddress;
	/* 0x10 */ DWORD		RawSize;
	/* 0x14 */ DWORD		SectionNameAddress;
	/* 0x18 */ DWORD		SectionNameReferenceCount;
	/* 0x1C */ DWORD		HeadSharedPageReferenceCount;
	/* 0x20 */ DWORD		TailSharedPageReferenceCount;
	/* 0x24 */ CHAR			SectionDigest[XBE_IMAGE_DIGEST_LENGTH];
};

struct XBOX_LIBRARY_VERSION
{
	/* 0x00 */ CHAR			LibraryName[8];
	/* 0x08 */ WORD			MajorVersion;
	/* 0x0A */ WORD			MinorVersion;
	/* 0x0C */ WORD			BuildVersion;
	/* 0x0E */ WORD			Flags;
};

struct XBE_IMAGE_IMPORT_DESCRIPTOR
{
	/* 0x00 */ DWORD		ImageThunkData;
	/* 0x04 */ DWORD		ModuleNameAddress;
};

// ---------------------------------------------------------------------------------------
// XboxExecutable
// ---------------------------------------------------------------------------------------
class XboxExecutable
{
private:
	std::string					sFileName;
	HANDLE						hFileHandle;

	bool						bIsValid;
	XBE_IMAGE_HEADER			sHeader;
	XBE_IMAGE_CERTIFICATE		sCertificate;

	XBE_IMAGE_SECTION_HEADER	*pSectionHeaders;
	std::vector<std::string>	vSectionHeaderNames;

	std::map<DWORD, std::wstring>	mImportDirectory;

	XBOX_LIBRARY_VERSION		*pLibraryVersions;
	XBOX_LIBRARY_VERSION		*pLibraryFeatures;

	std::string					sDebugFullFileName;
	std::wstring				sDebugFileNameUnicode;

	BYTE						*pbLogoBitmap;

public:
	XboxExecutable(std::string fileName);
	~XboxExecutable();

	bool ReadExecutable();

	bool AddSectionForHacks(std::string sectionName, int sectionSize);
};