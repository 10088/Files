#include "pch.h"
#include "MsgHandler_ContextMenu.h"
#include "base64.h"
#include <sstream>
#include <codecvt>

LRESULT CALLBACK WndProc(HWND hwnd, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
	MsgHandler_ContextMenu* pThis;
	if (uiMsg == WM_NCCREATE)
	{
		pThis = static_cast<MsgHandler_ContextMenu*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);

		SetLastError(0);
		if (!SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis)))
		{
			if (GetLastError() != 0)
				return FALSE;
		}
	}
	else if (uiMsg == WM_DESTROY)
	{
		PostQuitMessage(0);
		pThis = NULL;
	}
	else
	{
		pThis = reinterpret_cast<MsgHandler_ContextMenu*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}
	if (pThis)
	{
		if (uiMsg == WM_CONTEXTMENU)
		{
			pThis->LoadContextMenuForFile(reinterpret_cast<MenuArgs*>(lParam));
			SetEvent(reinterpret_cast<HANDLE>(wParam));
		}
		else if (pThis->LoadedContextMenu && pThis->LoadedContextMenu->g_pcm3)
		{
			LRESULT lres;
			if (SUCCEEDED(pThis->LoadedContextMenu->g_pcm3->HandleMenuMsg2(uiMsg, wParam, lParam, &lres)))
			{
				return lres;
			}
		}
		else if (pThis->LoadedContextMenu && pThis->LoadedContextMenu->g_pcm2)
		{
			if (SUCCEEDED(pThis->LoadedContextMenu->g_pcm2->HandleMenuMsg(uiMsg, wParam, lParam)))
			{
				return 0;
			}
		}
	}
	return DefWindowProc(hwnd, uiMsg, wParam, lParam);
}

void MsgHandler_ContextMenu::LoadContextMenuForFile(MenuArgs* args)
{
	if (this->LoadedContextMenu != NULL)
	{
		delete this->LoadedContextMenu;
	}
	this->LoadedContextMenu = new Win32ContextMenu();

	HWND hwnd = this->hiddenWindow;
	IContextMenu* pcm;
	if (SUCCEEDED(GetUIObjectOfFile(hwnd, args->FileList, IID_IContextMenu, (void**)&(pcm))))
	{
		this->LoadedContextMenu->cMenu = pcm;
		pcm->QueryInterface(IID_IContextMenu2, (void**)&(this->LoadedContextMenu->g_pcm2));
		pcm->QueryInterface(IID_IContextMenu3, (void**)&(this->LoadedContextMenu->g_pcm3));
		this->LoadedContextMenu->hMenu = CreatePopupMenu();
		if (this->LoadedContextMenu->hMenu)
		{
			if (SUCCEEDED(pcm->QueryContextMenu(this->LoadedContextMenu->hMenu, 0,
				SCRATCH_QCM_FIRST, SCRATCH_QCM_LAST,
				args->ExtendedMenu ? CMF_EXTENDEDVERBS : CMF_NORMAL)))
			{
				EnumMenuItems(this->LoadedContextMenu->cMenu, this->LoadedContextMenu->hMenu, this->LoadedContextMenu->Items);
			}
		}
	}
}

void MsgHandler_ContextMenu::EnumMenuItems(IContextMenu* cMenu, HMENU hMenu, std::vector<Win32ContextMenuItem>& menuItemsResult)
{
	auto itemCount = GetMenuItemCount(hMenu);
	MENUITEMINFO mii;
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_BITMAP | MIIM_FTYPE | MIIM_STRING | MIIM_ID | MIIM_SUBMENU;
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	for (int ii = 0; ii < itemCount; ii++)
	{
		Win32ContextMenuItem menuItem;
		wchar_t container[512];
		mii.dwTypeData = container;
		mii.cch = sizeof(container) / sizeof(*container) - 1; // https://devblogs.microsoft.com/oldnewthing/20040928-00/?p=37723
		auto retval = GetMenuItemInfo(hMenu, ii, true, &mii);
		if (!retval)
		{
			continue;
		}
		menuItem.Type = mii.fType;
		menuItem.ID = (int)mii.wID;
		if (menuItem.Type == MFT_STRING)
		{
			wprintf(L"Item %d (%d): %s\n", ii, mii.wID, mii.dwTypeData);
			menuItem.Label = myconv.to_bytes(mii.dwTypeData);
			CHAR pszName[512];
			// Hackish workaround to avoid an AccessViolationException on some items,
			// notably the "Run with graphic processor" menu item of NVidia cards
			if (mii.wID <= 5000)
			{
				if (SUCCEEDED(cMenu->GetCommandString(mii.wID - SCRATCH_QCM_FIRST, GCS_VERBA, NULL, pszName, 512)))
				{
					menuItem.CommandString = pszName;
				}
			}
			// TODO: Filter, skip items implemented in UWP
			if (mii.hbmpItem != NULL && mii.hbmpItem > HBMMENU_POPUP_MINIMIZE)
			{
				BITMAP nativeBitmap;
				Gdiplus::Bitmap* gdiBitmap = NULL;
				if (GetObject(mii.hbmpItem, sizeof(nativeBitmap), (LPVOID)&nativeBitmap) && nativeBitmap.bmBits)
				{
					gdiBitmap = new Gdiplus::Bitmap(nativeBitmap.bmWidth, nativeBitmap.bmWidth, PixelFormat32bppARGB);
					Gdiplus::BitmapData data;
					Gdiplus::Rect rect(0, 0, nativeBitmap.bmWidth, nativeBitmap.bmHeight);
					gdiBitmap->LockBits(&rect,
						Gdiplus::ImageLockModeWrite, gdiBitmap->GetPixelFormat(), &data);
					//memcpy(data.Scan0, nativeBitmap.bmBits, nativeBitmap.bmHeight * nativeBitmap.bmWidthBytes);
					for (int ll = 0; ll < nativeBitmap.bmHeight; ll++)
					{
						// Flip image upside-down, check winrar icon
						memcpy((char*)data.Scan0 + nativeBitmap.bmWidthBytes * ll, 
							(char*)nativeBitmap.bmBits + nativeBitmap.bmWidthBytes * (nativeBitmap.bmHeight - 1 - ll), 
							nativeBitmap.bmWidthBytes);
					}
					gdiBitmap->UnlockBits(&data);
				}
				else
				{
					gdiBitmap = new Gdiplus::Bitmap(mii.hbmpItem, nullptr);
				}

				if (gdiBitmap != NULL)
				{
					std::vector<BYTE> data;

					//write to IStream
					IStream* istream = nullptr;
					CreateStreamOnHGlobal(NULL, TRUE, &istream);

					CLSID clsid_png;
					CLSIDFromString(L"{557cf406-1a04-11d3-9a73-0000f81ef32e}", &clsid_png); // bmp: {557cf400-1a04-11d3-9a73-0000f81ef32e}
					Gdiplus::Status status = gdiBitmap->Save(istream, &clsid_png);
					if (status == Gdiplus::Status::Ok)
					{
						//get memory handle associated with istream
						HGLOBAL hg = NULL;
						GetHGlobalFromStream(istream, &hg);

						//copy IStream to buffer
						SIZE_T bufsize = GlobalSize(hg);
						data.resize(bufsize);

						//lock & unlock memory
						LPVOID pimage = GlobalLock(hg);
						memcpy(&data[0], pimage, bufsize);
						GlobalUnlock(hg);

						menuItem.IconBase64 = base64_encode(data.data(), data.size(), false);
					}

					istream->Release();
					//DeleteObject(mii.hbmpItem);
					delete gdiBitmap;
				}
			}
			if (mii.hSubMenu != NULL)
			{
				printf("Item %d: has submenu\n", ii);
				auto subItems = std::vector<Win32ContextMenuItem>();
				if (this->LoadedContextMenu->g_pcm3)
				{
					this->LoadedContextMenu->g_pcm3->HandleMenuMsg2(WM_INITMENUPOPUP, reinterpret_cast<LONG_PTR>(mii.hSubMenu), ii, NULL);
				}
				else if (this->LoadedContextMenu->g_pcm2)
				{
					this->LoadedContextMenu->g_pcm2->HandleMenuMsg(WM_INITMENUPOPUP, reinterpret_cast<LONG_PTR>(mii.hSubMenu), ii);
				}
				EnumMenuItems(cMenu, mii.hSubMenu, subItems);
				menuItem.SubItems = subItems;
				printf("Item %d: done submenu\n", ii);
			}
		}
		else
		{
			printf("Item %d: %d\n", ii, mii.fType);
		}
		menuItemsResult.push_back(menuItem);
	}
}

HRESULT MsgHandler_ContextMenu::GetUIObjectOfFile(HWND hwnd, std::vector<std::wstring> fileList, REFIID riid, void** ppv)
{
	*ppv = NULL;
	HRESULT hr;
	PIDLIST_ABSOLUTE pidl;
	SFGAOF sfgao;
	if (fileList.empty())
	{
		return -1;
	}

	auto firstElem = fileList[0].c_str();
	if (SUCCEEDED(hr = SHParseDisplayName(firstElem, NULL, &pidl, 0, &sfgao)))
	{
		IShellFolder* psf;
		if (SUCCEEDED(hr = SHBindToParent(pidl, IID_IShellFolder,
			(void**)&psf, NULL)))
		{
			PUITEMID_CHILD* rgpidl = (PUITEMID_CHILD*)CoTaskMemAlloc(sizeof(PUITEMID_CHILD) * fileList.size());
			if (rgpidl != NULL)
			{
				int parsedChildren = 0;
				for (; parsedChildren < fileList.size(); parsedChildren++)
				{
					PIDLIST_ABSOLUTE pidlChild;
					if (SUCCEEDED(hr = SHParseDisplayName(fileList[parsedChildren].c_str(), NULL, &pidlChild, 0, &sfgao)))
					{
						// if (ILFindChild(pidl, pidlChild) == NULL) // Not a child of parent folder
						rgpidl[parsedChildren] = (PUITEMID_CHILD)(ILClone(ILFindLastID(pidlChild)));
						CoTaskMemFree(pidlChild);
					}
					else
					{
						break;
					}
				}
				if (SUCCEEDED(hr))
				{
					hr = psf->GetUIObjectOf(hwnd, (UINT)fileList.size(), rgpidl, riid, NULL, ppv);
				}
				for (int freeIdx = 0; freeIdx < parsedChildren; freeIdx++)
				{
					CoTaskMemFree(rgpidl[freeIdx]);
				}
				CoTaskMemFree(rgpidl);
			}
			psf->Release();
		}
		CoTaskMemFree(pidl);
	}
	return hr;
}

IAsyncOperation<bool> MsgHandler_ContextMenu::ParseArgumentsAsync(AppServiceManager const& manager, AppServiceRequestReceivedEventArgs const& args)
{
	if (args.Request().Message().HasKey(L"Arguments"))
	{
		auto arguments = args.Request().Message().Lookup(L"Arguments").as<hstring>();
		if (arguments == L"LoadContextMenu")
		{
			auto filePath = args.Request().Message().Lookup(L"FilePath").as<hstring>();
			auto extendedMenu = args.Request().Message().Lookup(L"ExtendedMenu").as<bool>();
			auto showOpenMenu = args.Request().Message().Lookup(L"ShowOpenMenu").as<bool>();

			MenuArgs* menuArgs = new MenuArgs();
			menuArgs->ExtendedMenu = extendedMenu;
			menuArgs->ShowOpenMenu = showOpenMenu;
			std::wstringstream stringStream(filePath.c_str());
			std::wstring item;
			while (std::getline(stringStream, item, L'|'))
			{
				menuArgs->FileList.push_back(item);
			}

			if (this->hiddenWindow != NULL)
			{
				HANDLE ghMenuCloseEvent = CreateEvent(
					NULL, TRUE, FALSE, TEXT("MenuCloseEvent"));
				PostMessage(this->hiddenWindow, WM_CONTEXTMENU, reinterpret_cast<LONG_PTR>(ghMenuCloseEvent), reinterpret_cast<LONG_PTR>(menuArgs));
				WaitForSingleObject(ghMenuCloseEvent, INFINITE);
				CloseHandle(ghMenuCloseEvent);
				if (this->LoadedContextMenu != NULL)
				{
					auto serializedMenu = json(*this->LoadedContextMenu).dump();
					ValueSet valueSet;
					valueSet.Insert(L"Handle", winrt::box_value(L"HANDLE"));
					valueSet.Insert(L"ContextMenu", winrt::box_value(winrt::to_hstring(serializedMenu)));
					//printf("%s\n", serializedMenu.c_str());
					co_await args.Request().SendResponseAsync(valueSet);
				}
			}

			delete menuArgs;

			co_return TRUE;
		}
		else if (arguments == L"ExecAndCloseContextMenu")
		{
			if (this->LoadedContextMenu)
			{
				if (args.Request().Message().HasKey(L"ItemID"))
				{
					auto menuId = args.Request().Message().Lookup(L"ItemID").as<int>();
					this->InvokeCommand(menuId);
				}
				delete this->LoadedContextMenu;
				this->LoadedContextMenu = NULL;
			}
			co_return TRUE;
		}
	}
	co_return FALSE;
}

void MsgHandler_ContextMenu::CreateHiddenWindow(HINSTANCE hInstance)
{
	(void)CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	// Register the window class.
	const wchar_t CLASS_NAME[] = L"Files Window Class";

	WNDCLASS wc = { };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,                            // Optional window styles.
		CLASS_NAME,                   // Window class
		L"Files Fulltrust",           // Window text
		0,                            // Window style (WS_OVERLAPPEDWINDOW)
		0, 0, 0, 0,    // Size and position
		HWND_MESSAGE,  // Parent window (NULL)
		NULL,          // Menu
		wc.hInstance,  // Instance handle
		this           // Additional application data
	);

	ShowWindow(hwnd, SW_SHOW);

	this->hiddenWindow = hwnd;

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		//TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

MsgHandler_ContextMenu::MsgHandler_ContextMenu(HINSTANCE hInstance)
{
	this->windowThread = std::thread(&MsgHandler_ContextMenu::CreateHiddenWindow, this, hInstance);
}

MsgHandler_ContextMenu::~MsgHandler_ContextMenu()
{
	if (this->hiddenWindow != NULL)
	{
		PostMessage(this->hiddenWindow, WM_CLOSE, NULL, NULL);
	}
	if (this->windowThread.joinable())
	{
		this->windowThread.join();
	}
	delete this->LoadedContextMenu;
	this->LoadedContextMenu = NULL;
}

void MsgHandler_ContextMenu::InvokeCommand(int menuId)
{
	if (this->LoadedContextMenu)
	{
		CMINVOKECOMMANDINFOEX info = { 0 };
		info.cbSize = sizeof(info);
		info.fMask = CMIC_MASK_UNICODE;
		info.hwnd = this->hiddenWindow;
		info.lpVerb = MAKEINTRESOURCEA(menuId - SCRATCH_QCM_FIRST);
		info.lpVerbW = MAKEINTRESOURCEW(menuId - SCRATCH_QCM_FIRST);
		info.nShow = SW_SHOW;
		this->LoadedContextMenu->cMenu->InvokeCommand((LPCMINVOKECOMMANDINFO)&info);
	}
}

void MsgHandler_ContextMenu::InvokeCommand(std::string menuVerb)
{
	if (this->LoadedContextMenu)
	{
		CMINVOKECOMMANDINFOEX info = { 0 };
		info.cbSize = sizeof(info);
		//info.fMask = CMIC_MASK_UNICODE;
		info.hwnd = this->hiddenWindow;
		info.lpVerb = menuVerb.c_str();
		//info.lpVerbW = menuVerb.c_str();
		info.nShow = SW_SHOW;
		this->LoadedContextMenu->cMenu->InvokeCommand((LPCMINVOKECOMMANDINFO)&info);
	}
}