/*
 * Process Hacker -
 *   Appmodel support functions
 *
 * Copyright (C) 2017-2020 dmex
 *
 * This file is part of Process Hacker.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ph.h>
#include <lsasup.h>

#include <appmodel.h>
#include <shobjidl.h>

#include <appresolverp.h>
#include <appresolver.h>

static PVOID PhpQueryAppResolverInterface(
    VOID
    )
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static PVOID resolverInterface = NULL;

    if (PhBeginInitOnce(&initOnce))
    {
        if (WindowsVersion < WINDOWS_8)
            PhGetClassObject(L"appresolver.dll", &CLSID_StartMenuCacheAndAppResolver_I, &IID_IApplicationResolver61_I, &resolverInterface);
        else
            PhGetClassObject(L"appresolver.dll", &CLSID_StartMenuCacheAndAppResolver_I, &IID_IApplicationResolver62_I, &resolverInterface);

        PhEndInitOnce(&initOnce);
    }

    return resolverInterface;
}

static PVOID PhpQueryStartMenuCacheInterface(
    VOID
    )
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static PVOID startMenuInterface = NULL;

    if (PhBeginInitOnce(&initOnce))
    {
        if (WindowsVersion < WINDOWS_8)
            PhGetClassObject(L"appresolver.dll", &CLSID_StartMenuCacheAndAppResolver_I, &IID_IStartMenuAppItems61_I, &startMenuInterface);
        else
            PhGetClassObject(L"appresolver.dll", &CLSID_StartMenuCacheAndAppResolver_I, &IID_IStartMenuAppItems62_I, &startMenuInterface);

        PhEndInitOnce(&initOnce);
    }

    return startMenuInterface;
}

static LPMALLOC PhpQueryStartMenuMallocInterface(
    VOID
    )
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static LPMALLOC allocInterface = NULL;

    if (PhBeginInitOnce(&initOnce))
    {
        CoGetMalloc(MEMCTX_TASK, &allocInterface);

        PhEndInitOnce(&initOnce);
    }

    return allocInterface;
}

static BOOLEAN PhpKernelAppCoreInitialized(
    VOID
    )
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static BOOLEAN kernelAppCoreInitialized = FALSE;

    if (PhBeginInitOnce(&initOnce))
    {
        if (WindowsVersion >= WINDOWS_8)
        {
            PVOID kernelBaseModuleHandle;

            if (kernelBaseModuleHandle = PhLoadLibrarySafe(L"kernelbase.dll")) // kernel.appcore.dll
            {
                AppContainerDeriveSidFromMoniker_I = PhGetDllBaseProcedureAddress(kernelBaseModuleHandle, "AppContainerDeriveSidFromMoniker", 0);
                AppContainerLookupMoniker_I = PhGetDllBaseProcedureAddress(kernelBaseModuleHandle, "AppContainerLookupMoniker", 0);
                AppContainerFreeMemory_I = PhGetDllBaseProcedureAddress(kernelBaseModuleHandle, "AppContainerFreeMemory", 0);
            }

            if (
                AppContainerDeriveSidFromMoniker_I &&
                AppContainerLookupMoniker_I && 
                AppContainerFreeMemory_I
                )
            {
                kernelAppCoreInitialized = TRUE;
            }
        }

        PhEndInitOnce(&initOnce);
    }

    return kernelAppCoreInitialized;
}

_Success_(return)
BOOLEAN PhAppResolverGetAppIdForProcess(
    _In_ HANDLE ProcessId,
    _Out_ PPH_STRING *ApplicationUserModelId
    )
{
    PVOID resolverInterface;
    PWSTR appIdText = NULL;

    if (!(resolverInterface = PhpQueryAppResolverInterface()))
        return FALSE;

    if (WindowsVersion < WINDOWS_8)
    {
        IApplicationResolver_GetAppIDForProcess(
            (IApplicationResolver61*)resolverInterface,
            HandleToUlong(ProcessId),
            &appIdText,
            NULL,
            NULL,
            NULL
            );
    }
    else
    {
        IApplicationResolver2_GetAppIDForProcess(
            (IApplicationResolver62*)resolverInterface,
            HandleToUlong(ProcessId),
            &appIdText,
            NULL,
            NULL,
            NULL
            );
    }

    if (appIdText)
    {
        //SIZE_T appIdTextLength = IMalloc_GetSize(PhpQueryStartMenuMallocInterface(), appIdText);
        //*ApplicationUserModelId = PhCreateStringEx(appIdText, appIdTextLength - sizeof(UNICODE_NULL));
        //IMalloc_Free(PhpQueryStartMenuMallocInterface(), appIdText);

        *ApplicationUserModelId = PhCreateString(appIdText);
        CoTaskMemFree(appIdText);
        return TRUE;
    }

    return FALSE;
}

_Success_(return)
BOOLEAN PhAppResolverGetAppIdForWindow(
    _In_ HWND WindowHandle,
    _Out_ PPH_STRING *ApplicationUserModelId
    )
{
    PVOID resolverInterface;
    PWSTR appIdText = NULL;

    if (!(resolverInterface = PhpQueryAppResolverInterface()))
        return FALSE;

    if (WindowsVersion < WINDOWS_8)
    {
        IApplicationResolver_GetAppIDForWindow(
            (IApplicationResolver61*)resolverInterface,
            WindowHandle,
            &appIdText,
            NULL,
            NULL,
            NULL
            );
    }
    else
    {
        IApplicationResolver_GetAppIDForWindow(
            (IApplicationResolver62*)resolverInterface,
            WindowHandle,
            &appIdText,
            NULL,
            NULL,
            NULL
            );
    }

    if (appIdText)
    {
        //SIZE_T appIdTextLength = IMalloc_GetSize(PhpQueryStartMenuMallocInterface(), appIdText);
        //*ApplicationUserModelId = PhCreateStringEx(appIdText, appIdTextLength - sizeof(UNICODE_NULL));
        //IMalloc_Free(PhpQueryStartMenuMallocInterface(), appIdText);

        *ApplicationUserModelId = PhCreateString(appIdText);
        CoTaskMemFree(appIdText);
        return TRUE;
    }

    return FALSE;
}

HRESULT PhAppResolverActivateAppId(
    _In_ PPH_STRING AppUserModelId,
    _In_opt_ PWSTR CommandLine,
    _Out_opt_ HANDLE *ProcessId
    )
{
    HRESULT status;
    ULONG processId = ULONG_MAX;
    IApplicationActivationManager* applicationActivationManager;

    status = PhGetClassObject(
        L"twinui.appcore.dll",
        &CLSID_ApplicationActivationManager,
        &IID_IApplicationActivationManager,
        &applicationActivationManager
        );

    if (SUCCEEDED(status))
    {
        //CoAllowSetForegroundWindow((IUnknown*)applicationActivationManager, NULL);

        status = IApplicationActivationManager_ActivateApplication(
            applicationActivationManager,
            PhGetString(AppUserModelId),
            CommandLine,
            AO_NONE,
            &processId
            );

        IApplicationActivationManager_Release(applicationActivationManager);
    }

    if (SUCCEEDED(status))
    {
        if (ProcessId) *ProcessId = UlongToHandle(processId);
    }

    return status;
}

HRESULT PhAppResolverEnablePackageDebug(
    _In_ PPH_STRING PackageFullName
    )
{
    HRESULT status;
    IPackageDebugSettings* packageDebugSettings;

    status = PhGetClassObject(
        L"twinapi.appcore.dll",
        &CLSID_PackageDebugSettings,
        &IID_IPackageDebugSettings,
        &packageDebugSettings
        );

    if (SUCCEEDED(status))
    {
        status = IPackageDebugSettings_EnableDebugging(
            packageDebugSettings,
            PhGetString(PackageFullName),
            NULL,
            NULL
            );

        IPackageDebugSettings_Release(packageDebugSettings);
    }

    return status;
}

HRESULT PhAppResolverDisablePackageDebug(
    _In_ PPH_STRING PackageFullName
    )
{
    HRESULT status;
    IPackageDebugSettings* packageDebugSettings;

    status = PhGetClassObject(
        L"twinapi.appcore.dll",
        &CLSID_PackageDebugSettings,
        &IID_IPackageDebugSettings,
        &packageDebugSettings
        );

    if (SUCCEEDED(status))
    {
        status = IPackageDebugSettings_DisableDebugging(
            packageDebugSettings,
            PhGetString(PackageFullName)
            );

        IPackageDebugSettings_Release(packageDebugSettings);
    }

    return status;
}

HRESULT PhAppResolverPackageSuspend(
    _In_ PPH_STRING PackageFullName
    )
{
    HRESULT status;
    IPackageDebugSettings* packageDebugSettings;

    status = PhGetClassObject(
        L"twinapi.appcore.dll",
        &CLSID_PackageDebugSettings,
        &IID_IPackageDebugSettings,
        &packageDebugSettings
        );

    if (SUCCEEDED(status))
    {
        status = IPackageDebugSettings_Suspend(
            packageDebugSettings,
            PhGetString(PackageFullName)
            );

        IPackageDebugSettings_Release(packageDebugSettings);
    }

    return status;
}

HRESULT PhAppResolverPackageResume(
    _In_ PPH_STRING PackageFullName
    )
{
    HRESULT status;
    IPackageDebugSettings* packageDebugSettings;

    status = PhGetClassObject(
        L"twinapi.appcore.dll",
        &CLSID_PackageDebugSettings,
        &IID_IPackageDebugSettings,
        &packageDebugSettings
        );

    if (SUCCEEDED(status))
    {
        status = IPackageDebugSettings_Resume(
            packageDebugSettings,
            PhGetString(PackageFullName)
            );

        IPackageDebugSettings_Release(packageDebugSettings);
    }

    return status;
}

PPH_LIST PhAppResolverEnumeratePackageBackgroundTasks(
    _In_ PPH_STRING PackageFullName
    )
{
    HRESULT status;
    PPH_LIST packageTasks = NULL;
    IPackageDebugSettings* packageDebugSettings;

    status = PhGetClassObject(
        L"twinapi.appcore.dll",
        &CLSID_PackageDebugSettings,
        &IID_IPackageDebugSettings,
        &packageDebugSettings
        );

    if (SUCCEEDED(status))
    {
        ULONG taskCount = 0;
        PGUID taskIds = NULL;
        PWSTR* taskNames = NULL;

        status = IPackageDebugSettings_EnumerateBackgroundTasks(
            packageDebugSettings,
            PhGetString(PackageFullName),
            &taskCount,
            &taskIds,
            &taskNames
            );

        if (SUCCEEDED(status))
        {
            if (!packageTasks && taskCount > 0)
                packageTasks = PhCreateList(taskCount);

            for (ULONG i = 0; i < taskCount; i++)
            {
                PPH_PACKAGE_TASK_ENTRY entry;

                if (!packageTasks)
                    break;

                entry = PhAllocateZero(sizeof(PH_PACKAGE_TASK_ENTRY));
                entry->TaskGuid = taskIds[i];
                entry->TaskName = PhCreateString(taskNames[i]);

                PhAddItemList(packageTasks, entry);
            }
        }

        IPackageDebugSettings_Release(packageDebugSettings);
    }

    if (packageTasks)
        return packageTasks;
    else
        return NULL;
}

HRESULT PhAppResolverPackageStopSessionRedirection(
    _In_ PPH_STRING PackageFullName
    )
{
    HRESULT status;
    IPackageDebugSettings* packageDebugSettings;

    status = PhGetClassObject(
        L"twinapi.appcore.dll",
        &CLSID_PackageDebugSettings,
        &IID_IPackageDebugSettings,
        &packageDebugSettings
        );

    if (SUCCEEDED(status))
    {
        status = IPackageDebugSettings_StopSessionRedirection(
            packageDebugSettings,
            PhGetString(PackageFullName)
            );

        IPackageDebugSettings_Release(packageDebugSettings);
    }

    return status;
}

PPH_STRING PhGetAppContainerName(
    _In_ PSID AppContainerSid
    )
{
    HRESULT result;
    PPH_STRING appContainerName = NULL;
    PWSTR packageMonikerName;

    if (!PhpKernelAppCoreInitialized())
        return NULL;

    result = AppContainerLookupMoniker_I(
        AppContainerSid, 
        &packageMonikerName
        );

    if (SUCCEEDED(result))
    {
        appContainerName = PhCreateString(packageMonikerName);
        AppContainerFreeMemory_I(packageMonikerName);
    }
    else // Check the local system account appcontainer mappings. (dmex)
    {
        static PH_STRINGREF appcontainerMappings = PH_STRINGREF_INIT(L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppContainer\\Mappings\\");
        static PH_STRINGREF appcontainerDefaultMappings = PH_STRINGREF_INIT(L".DEFAULT\\");
        HANDLE keyHandle;
        PPH_STRING sidString;
        PPH_STRING keyPath;

        sidString = PhSidToStringSid(AppContainerSid);
        keyPath = PhConcatStringRef3(&appcontainerDefaultMappings, &appcontainerMappings, &sidString->sr);

        if (NT_SUCCESS(PhOpenKey(
            &keyHandle,
            KEY_READ,
            PH_KEY_USERS,
            &keyPath->sr,
            0
            )))
        {
            PhMoveReference(&appContainerName, PhQueryRegistryString(keyHandle, L"Moniker"));
            NtClose(keyHandle);
        }

        PhDereferenceObject(keyPath);
        PhDereferenceObject(sidString);
    }

    return appContainerName;
}

PPH_STRING PhGetAppContainerSidFromName(
    _In_ PWSTR AppContainerName
    )
{
    PSID appContainerSid;
    PPH_STRING packageSidString = NULL;

    if (!PhpKernelAppCoreInitialized())
        return NULL;

    if (SUCCEEDED(AppContainerDeriveSidFromMoniker_I(
        AppContainerName, 
        &appContainerSid
        )))
    {
        packageSidString = PhSidToStringSid(appContainerSid);

        RtlFreeSid(appContainerSid);
    }

    return packageSidString;
}

PPH_STRING PhGetAppContainerPackageName(
    _In_ PSID Sid
    )
{   
    static PH_STRINGREF appcontainerMappings = PH_STRINGREF_INIT(L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppContainer\\Mappings\\");
    static PH_STRINGREF appcontainerDefaultMappings = PH_STRINGREF_INIT(L".DEFAULT\\");
    HANDLE keyHandle;
    PPH_STRING sidString;
    PPH_STRING keyPath;
    PPH_STRING packageName = NULL;

    sidString = PhSidToStringSid(Sid);

    if (PhEqualString2(sidString, L"S-1-15-3-4096", FALSE)) // HACK
    {
        PhDereferenceObject(sidString);
        return PhCreateString(L"InternetExplorer");
    }

    keyPath = PhConcatStringRef2(&appcontainerMappings, &sidString->sr);

    if (NT_SUCCESS(PhOpenKey(
        &keyHandle,
        KEY_READ,
        PH_KEY_CURRENT_USER,
        &keyPath->sr,
        0
        )))
    {
        PhMoveReference(&packageName, PhQueryRegistryString(keyHandle, L"Moniker"));
        NtClose(keyHandle);
    }

    PhDereferenceObject(keyPath);

    // Check the local system account appcontainer mappings. (dmex)
    if (PhIsNullOrEmptyString(packageName))
    {
        keyPath = PhConcatStringRef3(&appcontainerDefaultMappings, &appcontainerMappings, &sidString->sr);

        if (NT_SUCCESS(PhOpenKey(
            &keyHandle,
            KEY_READ,
            PH_KEY_USERS,
            &keyPath->sr,
            0
            )))
        {
            PhMoveReference(&packageName, PhQueryRegistryString(keyHandle, L"Moniker"));
            NtClose(keyHandle);
        }

        PhDereferenceObject(keyPath);
    }

    PhDereferenceObject(sidString);

    return packageName;
}

PPH_STRING PhGetPackagePath(
    _In_ PPH_STRING PackageFullName
    )
{
    static PH_STRINGREF storeAppPackages = PH_STRINGREF_INIT(L"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages\\");
    HANDLE keyHandle;
    PPH_STRING keyPath;
    PPH_STRING packagePath = NULL;

    keyPath = PhConcatStringRef2(&storeAppPackages, &PackageFullName->sr);

    // rev from GetPackagePath
    if (NT_SUCCESS(PhOpenKey(
        &keyHandle,
        KEY_READ,
        PH_KEY_CURRENT_USER,
        &keyPath->sr,
        0
        )))
    {
        packagePath = PhQueryRegistryString(keyHandle, L"PackageRootFolder");
        NtClose(keyHandle);
    }

    PhDereferenceObject(keyPath);

    return packagePath;
}

PPH_STRING PhGetPackageAppDataPath(
    _In_ HANDLE ProcessHandle
    )
{
    static PH_STRINGREF attributeName = PH_STRINGREF_INIT(L"WIN://SYSAPPID");
    static PH_STRINGREF appdataPackages = PH_STRINGREF_INIT(L"%APPDATALOCAL%\\Packages\\");
    HANDLE tokenHandle;
    PTOKEN_SECURITY_ATTRIBUTES_INFORMATION info;
    PPH_STRING packageAppDataPath = NULL;

    if (NT_SUCCESS(PhOpenProcessToken(
        ProcessHandle,
        TOKEN_QUERY,
        &tokenHandle
        )))
    {
        if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenSecurityAttributes, &info)))
        {
            for (ULONG i = 0; i < info->AttributeCount; i++)
            {
                PTOKEN_SECURITY_ATTRIBUTE_V1 attribute = &info->Attribute.pAttributeV1[i];

                if (attribute->ValueType == TOKEN_SECURITY_ATTRIBUTE_TYPE_STRING)
                {
                    PH_STRINGREF attributeNameSr;

                    PhUnicodeStringToStringRef(&attribute->Name, &attributeNameSr);

                    if (PhEqualStringRef(&attributeNameSr, &attributeName, FALSE))
                    {
                        PPH_STRING attributeValue;
                        PPH_STRING attributePath;

                        attributeValue = PhCreateStringFromUnicodeString(&attribute->Values.pString[2]);

                        // TODO: Lookup user specific environment path from process. (dmex)
                        if (attributePath = PhExpandEnvironmentStrings(&appdataPackages))
                        {
                            packageAppDataPath = PhConcatStringRef2(&attributePath->sr, &attributeValue->sr);

                            PhDereferenceObject(attributePath);
                            PhDereferenceObject(attributeValue);
                            break;
                        }

                        PhDereferenceObject(attributeValue);
                    }
                }
            }

            PhFree(info);
        }

        NtClose(tokenHandle);
    }

    return packageAppDataPath;
}

PPH_STRING PhGetProcessPackageFullName(
    _In_ HANDLE ProcessHandle
    )
{
    static PH_STRINGREF attributeName = PH_STRINGREF_INIT(L"WIN://SYSAPPID");
    HANDLE tokenHandle;
    PTOKEN_SECURITY_ATTRIBUTES_INFORMATION info;
    PPH_STRING packageName = NULL;

    if (NT_SUCCESS(PhOpenProcessToken(
        ProcessHandle,
        TOKEN_QUERY,
        &tokenHandle
        )))
    {
        // rev from PackageIdFromFullName
        if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenSecurityAttributes, &info)))
        {
            for (ULONG i = 0; i < info->AttributeCount; i++)
            {
                PTOKEN_SECURITY_ATTRIBUTE_V1 attribute = &info->Attribute.pAttributeV1[i];

                if (attribute->ValueType == TOKEN_SECURITY_ATTRIBUTE_TYPE_STRING)
                {
                    PH_STRINGREF attributeNameSr;

                    PhUnicodeStringToStringRef(&attribute->Name, &attributeNameSr);

                    if (PhEqualStringRef(&attributeNameSr, &attributeName, FALSE))
                    {
                        packageName = PhCreateStringFromUnicodeString(&attribute->Values.pString[0]);
                        break;
                    }
                }
            }

            PhFree(info);
        }

        NtClose(tokenHandle);
    }

    return packageName;
}

BOOLEAN PhIsTokenFullTrustAppPackage(
    _In_ HANDLE TokenHandle
    )
{
    static PH_STRINGREF attributeName = PH_STRINGREF_INIT(L"WIN://SYSAPPID");
    PTOKEN_SECURITY_ATTRIBUTES_INFORMATION info;
    BOOLEAN tokenIsAppContainer = FALSE;
    BOOLEAN tokenHasAppId = FALSE;

    if (NT_SUCCESS(PhGetTokenIsAppContainer(TokenHandle, &tokenIsAppContainer)))
    {
        if (tokenIsAppContainer)
            return FALSE;
    }

    if (NT_SUCCESS(PhQueryTokenVariableSize(TokenHandle, TokenSecurityAttributes, &info)))
    {
        for (ULONG i = 0; i < info->AttributeCount; i++)
        {
            PTOKEN_SECURITY_ATTRIBUTE_V1 attribute = &info->Attribute.pAttributeV1[i];

            if (attribute->ValueType == TOKEN_SECURITY_ATTRIBUTE_TYPE_STRING)
            {
                PH_STRINGREF attributeNameSr;

                PhUnicodeStringToStringRef(&attribute->Name, &attributeNameSr);

                if (PhEqualStringRef(&attributeNameSr, &attributeName, FALSE))
                {
                    tokenHasAppId = TRUE;
                    break;
                }
            }
        }

        PhFree(info);
    }

    return tokenHasAppId;
}

BOOLEAN PhIsPackageCapabilitySid(
    _In_ PSID AppContainerSid,
    _In_ PSID Sid
    )
{
    BOOLEAN isPackageCapability = TRUE;

    for (ULONG i = 1; i < SECURITY_APP_PACKAGE_RID_COUNT - 1; i++)
    {
        if (
            *RtlSubAuthoritySid(AppContainerSid, i) !=
            *RtlSubAuthoritySid(Sid, i)
            )
        {
            isPackageCapability = FALSE;
            break;
        }
    }

    return isPackageCapability;
}

_Success_(return)
BOOLEAN PhGetAppWindowingModel(
    _In_ HANDLE ProcessTokenHandle,
    _Out_ AppPolicyWindowingModel *ProcessWindowingModelPolicy
    )
{
    if (!PhpKernelAppCoreInitialized() && !AppPolicyGetWindowingModel_I)
        return FALSE;

    return SUCCEEDED(AppPolicyGetWindowingModel_I(ProcessTokenHandle, ProcessWindowingModelPolicy));
}

PPH_LIST PhGetPackageAssetsFromResourceFile(
    _In_ PWSTR FilePath
    )
{
    IMrtResourceManager* resourceManager = NULL;
    IResourceMap* resourceMap = NULL;
    PPH_LIST resourceList = NULL;
    ULONG resourceCount = 0;

    if (FAILED(PhGetClassObject(
        L"mrmcorer.dll",
        &CLSID_MrtResourceManager_I,
        &IID_IMrtResourceManager_I,
        &resourceManager
        )))
    {
        return FALSE;
    }

    if (FAILED(IMrtResourceManager_InitializeForFile(resourceManager, FilePath)))
        goto CleanupExit;

    if (FAILED(IMrtResourceManager_GetMainResourceMap(resourceManager, &IID_IResourceMap_I, &resourceMap)))
        goto CleanupExit;

    if (FAILED(IResourceMap_GetNamedResourceCount(resourceMap, &resourceCount)))
        goto CleanupExit;

    resourceList = PhCreateList(10);

    for (ULONG i = 0; i < resourceCount; i++)
    {
        PWSTR resourceName;

        if (SUCCEEDED(IResourceMap_GetNamedResourceUri(resourceMap, i, &resourceName)))
        {
            PhAddItemList(resourceList, PhCreateString(resourceName));
        }
    }

CleanupExit:

    if (resourceMap)
        IResourceMap_Release(resourceMap);

    if (resourceManager)
        IMrtResourceManager_Release(resourceManager);

    return resourceList;
}

HRESULT PhAppResolverBeginCrashDumpTask(
    _In_ HANDLE ProcessId,
    _Out_ HANDLE *TaskHandle
    )
{
    HRESULT status;
    IOSTaskCompletion* taskCompletionManager;

    status = PhGetClassObject(
        L"twinapi.appcore.dll",
        &CLSID_OSTaskCompletion_I,
        &IID_IOSTaskCompletion_I,
        &taskCompletionManager
        );

    if (SUCCEEDED(status))
    {
        status = IOSTaskCompletion_BeginTask(
            taskCompletionManager,
            HandleToUlong(ProcessId),
            PT_TC_CRASHDUMP
            );
    }

    if (SUCCEEDED(status))
    {
        *TaskHandle = taskCompletionManager;
    }
    else if (taskCompletionManager)
    {
        IOSTaskCompletion_Release(taskCompletionManager);
    }

    return status;
}

HRESULT PhAppResolverEndCrashDumpTask(
    _In_ HANDLE TaskHandle
    )
{
    IOSTaskCompletion* taskCompletionManager = TaskHandle;
    HRESULT status;

    status = IOSTaskCompletion_EndTask(taskCompletionManager);
    IOSTaskCompletion_Release(taskCompletionManager);

    return status;
}

// TODO: FIXME
//HICON PhAppResolverGetPackageIcon(
//    _In_ HANDLE ProcessId,
//    _In_ PPH_STRING PackageFullName
//    )
//{
//    PVOID startMenuInterface;
//    PPH_STRING applicationUserModelId;
//    IPropertyStore* propStoreInterface;
//    HICON packageIcon = NULL;
//
//    if (!(startMenuInterface = PhpQueryStartMenuCacheInterface()))
//        return NULL;
//
//    if (!PhAppResolverGetAppIdForProcess(ProcessId, &applicationUserModelId))
//        return NULL;
//
//    if (WindowsVersion < WINDOWS_8)
//    {
//        IStartMenuAppItems_GetItem(
//            (IStartMenuAppItems61*)startMenuInterface,
//            SMAIF_DEFAULT,
//            applicationUserModelId->Buffer,
//            &IID_IPropertyStore,
//            &propStoreInterface
//            );
//    }
//    else
//    {
//        IStartMenuAppItems2_GetItem(
//            (IStartMenuAppItems62*)startMenuInterface,
//            SMAIF_DEFAULT,
//            applicationUserModelId->Buffer,
//            &IID_IPropertyStore,
//            &propStoreInterface
//            );
//    }
//
//    if (propStoreInterface)
//    {
//        IMrtResourceManager* mrtResourceManager;
//        IResourceMap* resourceMap;
//        PROPVARIANT propVar;
//        PWSTR filePath;
//
//        IPropertyStore_GetValue(propStoreInterface, &PKEY_Tile_Background, &propVar);
//        IPropertyStore_GetValue(propStoreInterface, &PKEY_Tile_SmallLogoPath, &propVar);
//
//        CoCreateInstance(
//            &CLSID_MrtResourceManager_I,
//            NULL,
//            CLSCTX_INPROC_SERVER,
//            &IID_IMrtResourceManager_I,
//            &mrtResourceManager
//            );
//
//        IMrtResourceManager_InitializeForPackage(mrtResourceManager, PackageFullName->Buffer);
//        IMrtResourceManager_GetMainResourceMap(mrtResourceManager, &IID_IResourceMap_I, &resourceMap);
//        IResourceMap_GetFilePath(resourceMap, propVar.pwszVal, &filePath);
//
//        //HBITMAP bitmap = PhLoadImageFromFile(filePath, 32, 32);
//        //packageIcon = PhBitmapToIcon(bitmap, 32, 32);
//
//        IResourceMap_Release(resourceMap);
//        IMrtResourceManager_Release(mrtResourceManager);
//        PropVariantClear(&propVar);
//
//        IPropertyStore_Release(propStoreInterface);
//    }
//
//    PhDereferenceObject(applicationUserModelId);
//
//    return packageIcon;
//}
