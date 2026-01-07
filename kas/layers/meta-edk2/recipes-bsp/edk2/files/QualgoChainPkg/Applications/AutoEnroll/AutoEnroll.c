#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FileHandleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/GlobalVariable.h>                 // gEfiGlobalVariableGuid (PK, KEK)
#include <Guid/ImageAuthentication.h>            // gEfiImageSecurityDatabaseGuid (db, dbx, dbr)
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

#define KEYS_DIR           L"\\keys\\"
#define MAX_KEYFILE_BYTES  (4*1024*1024)

// Properties Secure Boot (Time-based Auth Write!)
#define SB_ATTR (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | \
                 EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS)

typedef struct {
  CHAR16      *VarName;
  EFI_GUID    *VarGuid;
} VAR_DESC;

// M� t� bi�n c�n enroll
STATIC VAR_DESC VarDb  = { L"db",  (EFI_GUID*)&gEfiImageSecurityDatabaseGuid };
STATIC VAR_DESC VarDbx = { L"dbx", (EFI_GUID*)&gEfiImageSecurityDatabaseGuid };
STATIC VAR_DESC VarKEK = { L"KEK", (EFI_GUID*)&gEfiGlobalVariableGuid };
STATIC VAR_DESC VarPK  = { L"PK",  (EFI_GUID*)&gEfiGlobalVariableGuid };

//
// Utilities
//
STATIC
EFI_STATUS
ReadFileToBuffer(
  IN  EFI_HANDLE                 ImageHandle,
  IN  CHAR16                    *FullPath,
  OUT VOID                     **Buffer,
  OUT UINTN                     *BufferSize
  )
{
  EFI_STATUS                       Status;
  EFI_LOADED_IMAGE_PROTOCOL       *LoadedImage = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Sfsp = NULL;
  EFI_FILE_PROTOCOL               *Root = NULL;
  EFI_FILE_PROTOCOL               *File = NULL;
  EFI_FILE_INFO                   *Info = NULL;
  // UINTN                            InfoSize = 0;

  *Buffer = NULL;
  *BufferSize = 0;

  Status = gBS->OpenProtocol(ImageHandle,
                             &gEfiLoadedImageProtocolGuid,
                             (VOID**)&LoadedImage,
                             ImageHandle,
                             NULL,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  if (EFI_ERROR(Status)) return Status;

  Status = gBS->HandleProtocol(LoadedImage->DeviceHandle,
                               &gEfiSimpleFileSystemProtocolGuid,
                               (VOID**)&Sfsp);
  if (EFI_ERROR(Status)) return Status;

  Status = Sfsp->OpenVolume(Sfsp, &Root);
  if (EFI_ERROR(Status)) return Status;

  Status = Root->Open(Root, &File, FullPath, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(Status)) {
    Root->Close(Root);
    return Status;
  }

  // Get file size
  Info = FileHandleGetInfo(File);
  if (Info == NULL) {
    File->Close(File);
    Root->Close(Root);
    return EFI_NOT_FOUND;
  }


  if (Info->FileSize == 0 || Info->FileSize > MAX_KEYFILE_BYTES) {
    FreePool(Info);
    File->Close(File);
    Root->Close(Root);
    return EFI_BAD_BUFFER_SIZE;
  }

  *BufferSize = (UINTN)Info->FileSize;
  *Buffer = AllocateZeroPool(*BufferSize);
  if (*Buffer == NULL) {
    FreePool(Info);
    File->Close(File);
    Root->Close(Root);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = File->Read(File, BufferSize, *Buffer);
  FreePool(Info);
  File->Close(File);
  Root->Close(Root);
  return Status;
}

STATIC
EFI_STATUS
EnrollVariable(
  IN VAR_DESC  *Desc,
  IN VOID      *Data,
  IN UINTN      DataSize
  )
{
  EFI_STATUS Status;
  Status = gRT->SetVariable(
              Desc->VarName,
              Desc->VarGuid,
              SB_ATTR,
              DataSize,
              Data
           );
  return Status;
}

STATIC
VOID
PrintSbState(VOID)
{
  EFI_STATUS Status;
  UINT8      SetupMode = 0xFF;
  UINTN      Size = sizeof(SetupMode);

  Status = gRT->GetVariable(L"SetupMode", &gEfiGlobalVariableGuid, NULL, &Size, &SetupMode);
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] SetupMode: <unknown> (Status=%r)\r\n", Status);
  } else {
    Print(L"[AutoEnroll] SetupMode: %u (1=SetupMode, 0=UserMode)\r\n", (UINTN)SetupMode);
  }

  UINT8 SecureBoot = 0xFF;
  Size = sizeof(SecureBoot);
  Status = gRT->GetVariable(L"SecureBoot", &gEfiGlobalVariableGuid, NULL, &Size, &SecureBoot);
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] SecureBoot: <unknown> (Status=%r)\r\n", Status);
  } else {
    Print(L"[AutoEnroll] SecureBoot: %u\r\n", (UINTN)SecureBoot);
  }
}

STATIC
EFI_STATUS
TryEnrollFromOneFile(
  IN EFI_HANDLE  ImageHandle,
  IN VAR_DESC   *Desc,
  IN CHAR16     *Path
  )
{
  EFI_STATUS Status;
  VOID      *Buf = NULL;
  UINTN      Sz  = 0;

  Status = ReadFileToBuffer(ImageHandle, Path, &Buf, &Sz);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = EnrollVariable(Desc, Buf, Sz);
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] SetVariable %s failed for %s: %r\r\n", Desc->VarName, Path, Status);
  } else {
    Print(L"[AutoEnroll] Enrolled %s from %s (%u bytes)\r\n", Desc->VarName, Path, (UINTN)Sz);
  }
  if (Buf) FreePool(Buf);
  return Status;
}

STATIC
EFI_STATUS
TryEnrollByPattern(
  IN EFI_HANDLE  ImageHandle,
  IN VAR_DESC   *Desc,
  IN CHAR16     *Dir,         // e.g. L"\\keys\\"
  IN CHAR16     *BaseName     // e.g. L"db", "KEK", "PK", "dbx"
  )
{
  EFI_STATUS Status;
  CHAR16     Path[256];

  // �u ti�n .auth (Time-based Authenticated Variable)
  UnicodeSPrint(Path, sizeof(Path), L"%s%s.auth", Dir, BaseName);
  Status = TryEnrollFromOneFile(ImageHandle, Desc, Path);
  if (!EFI_ERROR(Status)) return Status;

  // Fall back .esl (ch� h�p l� � SetupMode)
  UnicodeSPrint(Path, sizeof(Path), L"%s%s.esl", Dir, BaseName);
  Status = TryEnrollFromOneFile(ImageHandle, Desc, Path);
  if (!EFI_ERROR(Status)) return Status;

  // Th� t�n th��ng g�p vi�t hoa
  UnicodeSPrint(Path, sizeof(Path), L"%s%s.AUTH", Dir, BaseName);
  Status = TryEnrollFromOneFile(ImageHandle, Desc, Path);
  if (!EFI_ERROR(Status)) return Status;

  UnicodeSPrint(Path, sizeof(Path), L"%s%s.ESL", Dir, BaseName);
  Status = TryEnrollFromOneFile(ImageHandle, Desc, Path);
  return Status;
}

//
// UefiMain: c� th� truy�n tu� ch�n:
//   AutoEnroll.efi            -> qu�t \keys\ tr�n c�ng volume
//   AutoEnroll.efi FS0:\keys\ -> ch� ��nh th� m�c kh�c
//   AutoEnroll.efi FS0:\my\dir PK KEK db -> ch� ��nh th� t�/t�n
//
EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status = EFI_SUCCESS;
  CHAR16    *Dir = KEYS_DIR;
  Print(L"[AutoEnroll] Automatic enroll (db -> KEK -> PK), folder=%s\r\n", Dir);
  PrintSbState();

  // 1) db (n�n enroll tr��c)
  Status = TryEnrollByPattern(ImageHandle, &VarDb, Dir, L"db");
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] db: no file or failed (%r). Continuing.\r\n", Status);
  }

  // 2) dbx (tu� ch�n)
  Status = TryEnrollByPattern(ImageHandle, &VarDbx, Dir, L"dbx");
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] dbx: no file or failed (%r). Continuing.\r\n", Status);
  }

  // 3) KEK
  Status = TryEnrollByPattern(ImageHandle, &VarKEK, Dir, L"KEK");
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] KEK: no file or failed (%r). Continuing.\r\n", Status);
  }

  // 4) PK (lu�n �� CU�I C�NG � enroll PK s� chuy�n SetupMode=0)
  Status = TryEnrollByPattern(ImageHandle, &VarPK, Dir, L"PK");
  if (EFI_ERROR(Status)) {
    Print(L"[AutoEnroll] PK: no file or failed (%r).\r\n", Status);
  }

  PrintSbState();
  Print(L"[AutoEnroll] Done.\r\n");
  return EFI_SUCCESS;
}

