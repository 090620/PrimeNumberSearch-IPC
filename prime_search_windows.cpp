#include <iostream>
#include <vector>
#include <windows.h>
#include <cmath>
#include <string>
#include <sstream>
#include <algorithm

const int MAX_NUM = 10000;
const int NUM_PROCESSES = 10;
const int INTERVAL = MAX_NUM / NUM_PROCESSES;
const int MAX_PATH_LENGTH = 32767;

bool is_prime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i = i + 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

void ChildProcess(HANDLE hReadPipe, HANDLE hWritePipe) {
    DWORD dwRead;
    int start, end;
    std::vector<int> primes;

    if (!ReadFile(hReadPipe, &start, sizeof(int), &dwRead, NULL) || dwRead == 0) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return;
    }
    if (!ReadFile(hReadPipe, &end, sizeof(int), &dwRead, NULL) || dwRead == 0) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return;
    }

    for (int i = start; i < end; ++i) {
        if (is_prime(i)) {
            primes.push_back(i);
        }
    }

    DWORD dwWritten;
    int count = primes.size();

    WriteFile(hWritePipe, &count, sizeof(int), &dwWritten, NULL);

    if (count > 0) {
        WriteFile(hWritePipe, primes.data(), count * sizeof(int), &dwWritten, NULL);
    }

    CloseHandle(hReadPipe);
    CloseHandle(hWritePipe);
}

int ParentProcess() {
    std::cout << "Porneste cautarea de numere prime pana la " << MAX_NUM
        << " folosind " << NUM_PROCESSES << " procese.\n";

    wchar_t szExecutablePath[MAX_PATH_LENGTH];
    GetModuleFileNameW(NULL, szExecutablePath, MAX_PATH_LENGTH);

    HANDLE hParentRead[NUM_PROCESSES];
    HANDLE hParentWrite[NUM_PROCESSES];
    HANDLE hChildRead[NUM_PROCESSES];
    HANDLE hChildWrite[NUM_PROCESSES];
    PROCESS_INFORMATION pi[NUM_PROCESSES] = { 0 };
    STARTUPINFO si[NUM_PROCESSES] = { 0 };
    std::vector<int> all_primes;

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        si[i].cb = sizeof(STARTUPINFO);

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&hChildRead[i], &hParentWrite[i], &sa, 0)) {
            std::cerr << "Eroare la crearea pipe-ului P->C.\n"; return 1;
        }

        if (!CreatePipe(&hParentRead[i], &hChildWrite[i], &sa, 0)) {
            std::cerr << "Eroare la crearea pipe-ului C->P.\n"; return 1;
        }

        int start = i * INTERVAL + 1;
        int end = (i + 1) * INTERVAL + 1;

        std::wstringstream ws;
        ws << L"\"" << szExecutablePath << L"\" CHILD "
            << (DWORD)hChildRead[i] << L" " << (DWORD)hChildWrite[i];

        std::wstring wsCmd = ws.str();
        wchar_t* szCmd = new wchar_t[wsCmd.length() + 1];
        wcscpy_s(szCmd, wsCmd.length() + 1, wsCmd.c_str());

        if (!CreateProcessW(
            NULL,
            szCmd,
            NULL,
            NULL,
            TRUE,
            0,
            NULL,
            NULL,
            &si[i],
            &pi[i]
        )) {
            DWORD dwError = GetLastError();
            std::cerr << "Eroare la crearea procesului copil. Cod eroare: " << dwError << "\n";
            delete[] szCmd;
            CloseHandle(hParentWrite[i]);
            CloseHandle(hChildRead[i]);
            CloseHandle(hParentRead[i]);
            CloseHandle(hChildWrite[i]);
            return 1;
        }
        delete[] szCmd;

        CloseHandle(hChildRead[i]);
        CloseHandle(hChildWrite[i]);

        DWORD dwWritten;
        WriteFile(hParentWrite[i], &start, sizeof(int), &dwWritten, NULL);
        WriteFile(hParentWrite[i], &end, sizeof(int), &dwWritten, NULL);

        CloseHandle(hParentWrite[i]);
    }

    std::cout << "\nNumerele prime gasite:\n";
    DWORD dwRead;

    for (int i = 0; i < NUM_PROCESSES; ++i) {
        WaitForSingleObject(pi[i].hProcess, INFINITE);

        int count = 0;

        if (ReadFile(hParentRead[i], &count, sizeof(int), &dwRead, NULL) && dwRead > 0) {
            if (count > 0) {
                std::vector<int> primes(count);
                ReadFile(hParentRead[i], primes.data(), count * sizeof(int), &dwRead, NULL);
                all_primes.insert(all_primes.end(), primes.begin(), primes.end());
            }
        }

        CloseHandle(hParentRead[i]);
        CloseHandle(pi[i].hProcess);
        CloseHandle(pi[i].hThread);
    }

    std::sort(all_primes.begin(), all_primes.end());
    for (int prime : all_primes) {
        std::cout << prime << " ";
    }

    std::cout << "\n\nTotal numere prime gasite: " << all_primes.size() << "\n";
    return 0;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc > 1 && wcscmp(argv[1], L"CHILD") == 0) {
        HANDLE hReadPipe = (HANDLE)_wtoi(argv[2]);
        HANDLE hWritePipe = (HANDLE)_wtoi(argv[3]);
        ChildProcess(hReadPipe, hWritePipe);
        return 0;
    }
    else {
        return ParentProcess();
    }
}