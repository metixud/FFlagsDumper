#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <regex>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <cstdint>
#include <array>
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wininet.h>
#include <tlhelp32.h>
#include <psapi.h>

#pragma comment(lib, "wininet.lib")

auto hex_enabled = true;

static auto cwc(void* contents, size_t size, size_t nmemb, void* userp) -> size_t
{
    auto total_size = size * nmemb;
    auto* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), total_size);
    return total_size;
}

auto dfv() -> std::string
{
    auto result = std::string();
    auto h_internet = InternetOpenA("FFlagDumper", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!h_internet)
    {
        return result;
    }
    auto h_url = InternetOpenUrlA(h_internet, "https://raw.githubusercontent.com/MaximumADHD/Roblox-Client-Tracker/refs/heads/roblox/FVariables.txt", nullptr, 0, INTERNET_FLAG_RELOAD, 0);
    if (!h_url)
    {
        InternetCloseHandle(h_internet);
        return result;
    }
    char buffer[4096];
    auto bytes_read = DWORD(0);
    while (InternetReadFile(h_url, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0)
    {
        result.append(buffer, bytes_read);
    }
    InternetCloseHandle(h_url);
    InternetCloseHandle(h_internet);
    return result;
}

auto gaf(const std::string& data) -> std::unordered_map<std::string, std::string>
{
    auto flags = std::unordered_map<std::string, std::string>();
    auto ss = std::istringstream(data);
    auto line = std::string();
    auto re = std::regex("^(SF|DF|F)(Flag|Int|String|Log)");

    while (std::getline(ss, line))
    {
        auto parts = std::vector<std::string>();
        auto ls = std::istringstream(line);
        auto token = std::string();
        while (ls >> token)
        {
            parts.push_back(token);
        }
        if (parts.size() < 2)
        {
            continue;
        }
        auto& second = parts[1];
        auto matches = std::smatch();
        if (std::regex_search(second, matches, re) && matches.size() == 3)
        {
            auto base_name = std::regex_replace(second, re, "");
            if (!base_name.empty())
            {
                flags[base_name] = matches[1].str() + matches[2].str();
            }
        }
    }

    return flags;
}

auto gpid(const std::wstring& process_name) -> DWORD
{
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    auto entry = PROCESSENTRY32W{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, process_name.c_str()) == 0)
            {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return 0;
}

auto gmb(HANDLE process, const std::wstring& module_name) -> uintptr_t
{
    auto modules = std::array<HMODULE, 1024>{};
    auto needed = DWORD(0);
    if (!EnumProcessModules(process, modules.data(), sizeof(modules), &needed))
    {
        return 0;
    }
    auto count = needed / sizeof(HMODULE);
    for (size_t i = 0; i < count; i++)
    {
        auto name = std::array<wchar_t, MAX_PATH>{};
        if (GetModuleBaseNameW(process, modules[i], name.data(), MAX_PATH))
        {
            if (_wcsicmp(name.data(), module_name.c_str()) == 0)
            {
                return reinterpret_cast<uintptr_t>(modules[i]);
            }
        }
    }
    return 0;
}

auto gms(HANDLE process, uintptr_t base) -> size_t
{
    auto info = MODULEINFO{};
    if (GetModuleInformation(process, reinterpret_cast<HMODULE>(base), &info, sizeof(info)))
    {
        return info.SizeOfImage;
    }
    return 0;
}

auto rpmb(HANDLE process, uintptr_t base, size_t size) -> std::vector<uint8_t>
{
    auto buffer = std::vector<uint8_t>(size, 0);
    auto chunk_size = size_t(0x1000);
    auto total_read = size_t(0);
    for (size_t offset = 0; offset < size; offset += chunk_size)
    {
        auto to_read = (offset + chunk_size > size) ? (size - offset) : chunk_size;
        auto bytes_read = SIZE_T(0);
        if (ReadProcessMemory(process, reinterpret_cast<LPCVOID>(base + offset), buffer.data() + offset, to_read, &bytes_read))
        {
            total_read += bytes_read;
        }
    }
    if (total_read == 0)
    {
        return {};
    }
    return buffer;
}

auto ar(const std::vector<uint8_t>& data, size_t address) -> int64_t
{
    if (address + 7 > data.size())
    {
        return -1;
    }
    auto offset = int32_t();
    std::memcpy(&offset, data.data() + address + 3, sizeof(int32_t));
    auto result = static_cast<int64_t>(address) + 7 + static_cast<int64_t>(offset);
    return result;
}

auto rs(const std::vector<uint8_t>& data, int64_t offset) -> std::string
{
    if (offset < 0 || static_cast<size_t>(offset) >= data.size())
    {
        return {};
    }
    auto end = static_cast<size_t>(offset);
    while (end < data.size() && data[end] != 0)
    {
        end++;
    }
    return std::string(reinterpret_cast<const char*>(data.data() + offset), end - static_cast<size_t>(offset));
}

auto evn(const std::vector<uint8_t>& data) -> std::string
{
    auto needle = std::string("RobloxPlayerBeta.exe");
    auto hay = std::string(reinterpret_cast<const char*>(data.data()), data.size());
    auto pos = hay.find(needle);
    if (pos == std::string::npos)
    {
        return {};
    }
    auto version_tag = std::string("version-");
    auto ver_pos = hay.rfind(version_tag, pos);
    if (ver_pos == std::string::npos)
    {
        return {};
    }
    auto end = ver_pos;
    while (end < hay.size() && hay[end] != '\\' && hay[end] != '\0')
    {
        end++;
    }
    if (end <= ver_pos)
    {
        return {};
    }
    return hay.substr(ver_pos, end - ver_pos);
}

auto __fastcall main(int argc, char** argv) -> int32_t
{
    auto timeout_seconds = int32_t(0);
    if (argc > 1)
    {
        auto arg = std::string(argv[1]);
        if (arg == "--timeout" && argc > 2)
        {
            timeout_seconds = std::stoi(argv[2]);
            std::cout << "timeout mode: " << timeout_seconds << " seconds" << std::endl;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "usage: " << argv[0] << " [--timeout <seconds>]" << std::endl;
            std::cout << "  --timeout <seconds>  dump continuously for X seconds" << std::endl;
            return int32_t();
        }
    }

    auto pid = gpid(L"RobloxPlayerBeta.exe");
    if (pid == 0)
    {
        std::cout << "roblox not running" << std::endl;
        std::cout << "press enter to close..." << std::endl;
        std::cin.get();
        return int32_t();
    }

    auto process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!process)
    {
        std::cout << "failed to open process" << std::endl;
        std::cout << "press enter to close..." << std::endl;
        std::cin.get();
        return int32_t();
    }

    auto base = gmb(process, L"RobloxPlayerBeta.exe");
    if (base == 0)
    {
        std::cout << "failed to get module base" << std::endl;
        CloseHandle(process);
        std::cout << "press enter to close..." << std::endl;
        std::cin.get();
        return int32_t();
    }

    auto size = gms(process, base);
    if (size == 0)
    {
        std::cout << "failed to get module size" << std::endl;
        CloseHandle(process);
        std::cout << "press enter to close..." << std::endl;
        std::cin.get();
        return int32_t();
    }

    std::cout << "reading " << size << " bytes from roblox..." << std::endl;
    auto file_data = rpmb(process, base, size);
    CloseHandle(process);
    if (file_data.empty())
    {
        std::cout << "failed to read process memory" << std::endl;
        std::cout << "press enter to close..." << std::endl;
        std::cin.get();
        return int32_t();
    }
    std::cout << "read " << file_data.size() << " bytes" << std::endl;

    auto version_name = evn(file_data);
    auto out_txt_path = std::string("fflags.txt");
    auto out_json_path = std::string("fflags.json");
    auto out_hpp_path = std::string("fflags.hpp");
    auto out_csv_path = std::string("fflags.csv");
    if (!version_name.empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(version_name, ec);
        if (!ec)
        {
            out_txt_path = version_name + "\\fflags.txt";
            out_json_path = version_name + "\\fflags.json";
            out_hpp_path = version_name + "\\fflags.hpp";
            out_csv_path = version_name + "\\fflags.csv";
        }
    }

    auto fvars = dfv();
    if (fvars.empty())
    {
        std::cout << "failed to download FVariables" << std::endl;
        return int32_t();
    }

    auto flags = gaf(fvars);
    auto all_found_flags = std::map<std::string, std::pair<std::string, int64_t>>();

    auto start = std::chrono::high_resolution_clock::now();
    auto end_time = start + std::chrono::seconds(timeout_seconds);

    auto pattern = std::array<uint8_t, 3>{ 0x48, 0x8D, 0x15 };
    auto scan_count = int32_t(0);

    if (timeout_seconds > 0)
    {
        std::cout << "scanning for " << timeout_seconds << " seconds..." << std::endl;
        while (std::chrono::high_resolution_clock::now() < end_time)
        {
            scan_count++;
            std::cout << "scan " << scan_count << "..." << std::endl;

            auto pos = size_t(0);
            auto found = bool(false);

            while (true)
            {
                found = false;
                for (; pos + pattern.size() <= file_data.size(); pos++)
                {
                    if (file_data[pos] == pattern[0] && file_data[pos + 1] == pattern[1] && file_data[pos + 2] == pattern[2])
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    break;
                }

                auto target = ar(file_data, pos);

                if (target != -1 && static_cast<size_t>(target) < file_data.size() && pos >= 13)
                {
                    if (file_data[pos - 13] == 0x4C && file_data[pos - 12] == 0x8D && file_data[pos - 11] == 0x05)
                    {
                        auto flag_name = rs(file_data, target);
                        auto it = flags.find(flag_name);
                        if (it != flags.end())
                        {
                            auto flag_address = ar(file_data, pos - 13);
                            if (flag_address != -1)
                            {
                                all_found_flags[flag_name] = std::make_pair(it->second, flag_address);
                            }
                        }
                    }
                }

                pos++;
                if (pos >= file_data.size())
                {
                    break;
                }
            }

            file_data = rpmb(OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid), base, size);
            if (file_data.empty())
            {
                std::cout << "failed to re-read memory" << std::endl;
                break;
            }

            Sleep(1000);
        }
        std::cout << "timeout reached, found " << all_found_flags.size() << " unique flags" << std::endl;
    }
    else
    {
        auto pos = size_t(0);
        auto found = bool(false);

        while (true)
        {
            found = false;
            for (; pos + pattern.size() <= file_data.size(); pos++)
            {
                if (file_data[pos] == pattern[0] && file_data[pos + 1] == pattern[1] && file_data[pos + 2] == pattern[2])
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                break;
            }

            auto target = ar(file_data, pos);

            if (target != -1 && static_cast<size_t>(target) < file_data.size() && pos >= 13)
            {
                if (file_data[pos - 13] == 0x4C && file_data[pos - 12] == 0x8D && file_data[pos - 11] == 0x05)
                {
                    auto flag_name = rs(file_data, target);
                    auto it = flags.find(flag_name);
                    if (it != flags.end())
                    {
                        auto flag_address = ar(file_data, pos - 13);
                        if (flag_address != -1)
                        {
                            all_found_flags[flag_name] = std::make_pair(it->second, flag_address);
                        }
                    }
                }
            }

            pos++;
            if (pos >= file_data.size())
            {
                break;
            }
        }
    }

    auto out_txt_file = std::ofstream(out_txt_path);
    if (!out_txt_file.is_open())
    {
        std::cout << "failed to open fflags.txt" << std::endl;
        return int32_t();
    }
    auto out_json_file = std::ofstream(out_json_path);
    if (!out_json_file.is_open())
    {
        std::cout << "failed to open fflags.json" << std::endl;
        return int32_t();
    }
    auto out_hpp_file = std::ofstream(out_hpp_path);
    if (!out_hpp_file.is_open())
    {
        std::cout << "failed to open fflags.hpp" << std::endl;
        return int32_t();
    }
    auto out_csv_file = std::ofstream(out_csv_path);
    if (!out_csv_file.is_open())
    {
        std::cout << "failed to open fflags.csv" << std::endl;
        return int32_t();
    }

    auto duration = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();
    auto total_flags = static_cast<int32_t>(all_found_flags.size());
    struct tm time_info;
    auto current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    localtime_s(&time_info, &current_time);
    auto time_stream = std::ostringstream();
    time_stream << std::put_time(&time_info, "%H:%M %d/%m/%Y (EU TIME)");
    auto time_str = time_stream.str();
    auto header_label = "Metix FFlags Dumper selfleaked version";
    auto time_taken_str = std::to_string(duration) + "s";

    out_txt_file << "/*" << std::endl;
    out_txt_file << " * Dumped With: " << header_label << std::endl;
    out_txt_file << " * Roblox Version: " << version_name << std::endl;
    out_txt_file << " * Dumped at: " << time_str << std::endl;
    out_txt_file << " * Time Taken: " << time_taken_str << std::endl;
    out_txt_file << " * Total FFlags: " << total_flags << std::endl;
    out_txt_file << " */" << std::endl << std::endl;

    out_json_file << "{" << std::endl;
    out_json_file << "  \"header\": {" << std::endl;
    out_json_file << "    \"dumped_with\": \"" << header_label << "\"," << std::endl;
    out_json_file << "    \"roblox_version\": \"" << version_name << "\"," << std::endl;
    out_json_file << "    \"dumped_at\": \"" << time_str << "\"," << std::endl;
    out_json_file << "    \"time_taken\": \"" << time_taken_str << "\"," << std::endl;
    out_json_file << "    \"total_fflags\": \"" << total_flags << "\"" << std::endl;
    out_json_file << "  }," << std::endl;
    out_json_file << "  \"fflags\": [" << std::endl;

    out_hpp_file << "#pragma once" << std::endl << "#include <string>" << std::endl << std::endl;
    out_hpp_file << "/*" << std::endl;
    out_hpp_file << " * Dumped With: " << header_label << std::endl;
    out_hpp_file << " * Roblox Version: " << version_name << std::endl;
    out_hpp_file << " * Dumped at: " << time_str << std::endl;
    out_hpp_file << " * Time Taken: " << time_taken_str << std::endl;
    out_hpp_file << " * Total FFlags: " << total_flags << std::endl;
    out_hpp_file << " */" << std::endl << std::endl;
    out_hpp_file << "using FLog = uintptr_t;" << std::endl;
    out_hpp_file << "using FFlag = uintptr_t;" << std::endl;
    out_hpp_file << "using FInt = uintptr_t;" << std::endl;
    out_hpp_file << "using FString = uintptr_t;" << std::endl;
    out_hpp_file << "using DFLog = uintptr_t;" << std::endl;
    out_hpp_file << "using DFFlag = uintptr_t;" << std::endl;
    out_hpp_file << "using DFInt = uintptr_t;" << std::endl;
    out_hpp_file << "using DFString = uintptr_t;" << std::endl;
    out_hpp_file << "using SFLog = uintptr_t;" << std::endl;
    out_hpp_file << "using SFFlag = uintptr_t;" << std::endl;
    out_hpp_file << "using SFInt = uintptr_t;" << std::endl;
    out_hpp_file << "using SFString = uintptr_t;" << std::endl;
    out_hpp_file << std::endl << "namespace fflags {" << std::endl;

    out_csv_file << "# Dumped With: " << header_label << std::endl;
    out_csv_file << "# Roblox Version: " << version_name << std::endl;
    out_csv_file << "# Dumped at: " << time_str << std::endl;
    out_csv_file << "# Time Taken: " << time_taken_str << std::endl;
    out_csv_file << "# Total FFlags: " << total_flags << std::endl;
    out_csv_file << "type,name,address" << std::endl;

    auto printed_flags = int32_t(0);
    auto first_entry = true;
    auto format_hex = [](int64_t value) {
        auto stream = std::ostringstream();
        stream << std::hex << std::nouppercase << value;
        return stream.str();
        };

    for (const auto& [flag_name, flag_data] : all_found_flags)
    {
        auto hex_address = format_hex(flag_data.second);
        printed_flags++;

        if (!first_entry)
        {
            out_txt_file << std::endl;
            out_json_file << "," << std::endl;
        }
        first_entry = false;

        out_txt_file << flag_data.first << " " << flag_name << " = 0x" << hex_address;
        out_json_file << "        {\"type\": \"" << flag_data.first << "\", \"name\": \"" << flag_name << "\", \"address\": \"0x" << hex_address << "\"}";
        out_hpp_file << "    inline constexpr " << flag_data.first << " " << flag_name << " = 0x" << hex_address << ";" << std::endl;
        out_csv_file << flag_data.first << "," << flag_name << ",0x" << hex_address << std::endl;
    }

    out_json_file << std::endl << "    ]" << std::endl << "}" << std::endl;
    out_hpp_file << "}" << std::endl;

    std::cout << "found " << printed_flags << "/" << flags.size() << " fflags" << std::endl;
    std::cout << "saved to " << out_txt_path << ", " << out_json_path << ", " << out_hpp_path << ", " << out_csv_path << std::endl;
    std::cout << "took " << duration << "s" << std::endl;

    if (timeout_seconds == 0)
    {
        std::cout << "Press Enter to exit...";
        std::cin.get();
    }

    return 0;
}