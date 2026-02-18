#include "vsp_bridge.h"

#include <QString>
#include <windows.h>
#include <comdef.h>

// Підтягуємо COM-типи HHD VSPT.
// auto_search пробує знайти зареєстровану type library в системі.
#import "hhdvspkit.tlb" named_guids, auto_search

bool createLocalBridge_COM(int comA, int comB, QString* errorText)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool needUninit = SUCCEEDED(hr);

    try
    {
        hhdvspkit::ISerialPortLibraryPtr library;
        // У експорті в них так само: CreateInstance(__uuidof(...))
        library.CreateInstance(__uuidof(hhdvspkit::SerialPortLibrary));

        // створюємо 2 bridge-порти
        auto port1 = library->createBridgePort(comA);
        auto port2 = library->createBridgePort(comB);

        // з’єднуємо
        port1->bridgePort = comB;
        port2->bridgePort = comA;

        if (needUninit) CoUninitialize();
        return true;
    }
    catch (const _com_error& e)
    {
        if (errorText)
            *errorText = QString("COM error: %1 (0x%2)")
                             .arg(QString::fromWCharArray(e.ErrorMessage()))
                             .arg(uint(e.Error()), 8, 16, QLatin1Char('0'));

        if (needUninit) CoUninitialize();
        return false;
    }
    catch (...)
    {
        if (errorText) *errorText = "Unknown error while creating bridge.";
        if (needUninit) CoUninitialize();
        return false;
    }
}
