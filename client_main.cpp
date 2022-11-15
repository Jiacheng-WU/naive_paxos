//
// Created by Jiacheng Wu on 10/6/22.
//

#include <iostream>
#include "client.hpp"
#include <fstream>
#include "magic_enum.hpp"
#include <chrono>
#include <thread>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/lexical_cast.hpp>

std::tuple<std::uint16_t, std::string, bool> parse_argument(int argc, char* argv[]) {
    std::uint16_t port_number = 0;
    std::string config_filepath_str = "../config.json";
    bool is_interactive = false;
    if (argc == 1 || argc >= 5) {
        fmt::print("{} {} {} {}\n", "program_name", "(port = 0)", "(config path)", "(interactive)");
        exit(0);
    } else  {
        try {
            port_number = boost::lexical_cast<uint16_t>(argv[1]);
            if (port_number != 0 && (port_number < 1024 || port_number > 49151)) {
                BOOST_LOG_TRIVIAL(warning) << fmt::format("Port {} is not a registered port in [1024, 49151], set to 0 then!\n", port_number);
                port_number = 0;
            }
        } catch (boost::bad_lexical_cast& err) {
            fmt::print("{} {} {} {}\n", "program_name", "(port = 0)", "(config path)", "(interactive)");
            exit(0);
        }

        if (argc >= 3) {
            config_filepath_str = std::string(argv[2]);
        }

        if (argc >= 4) {
            std::string itc = std::string(argv[3]);
            if (itc == "interactive") {
                is_interactive = true;
            } else {
                fmt::print("{} {} {} {}\n", "program_name", "(port = 0)", "(config path)", "(interactive)");
                exit(0);
            }
        }
    }
    return {port_number, config_filepath_str, is_interactive};
}

int main(int argc, char* argv[]) {

    auto [port_number, config_filepath_str, is_interactive] = parse_argument(argc, argv);


    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->load_config(std::filesystem::path(config_filepath_str));

    if (config->at_most_once && port_number == 0) {
        BOOST_LOG_TRIVIAL(warning) << fmt::format("Not registered port may not support at-most once semantics!\n", port_number);
    }

    PaxosClient client(std::move(config), port_number);
    // boost::system::error_code error;

    if (!is_interactive) {
        fmt::print("lock(1)\t response: {}\n", magic_enum::enum_name(client.lock(1).first));
        fmt::print("lock(1)\t response: {}\n", magic_enum::enum_name(client.lock(1).first));

        fmt::print("lock(2)\t response: {}\n", magic_enum::enum_name(client.lock(2).first));
        fmt::print("lock(2)\t response: {}\n", magic_enum::enum_name(client.lock(2).first));

        std::this_thread::sleep_for(std::chrono::seconds(10));

        fmt::print("unlock(2)\t response: {}\n", magic_enum::enum_name(client.unlock(2).first));
        fmt::print("unlock(2)\t response: {}\n", magic_enum::enum_name(client.unlock(2).first));

        fmt::print("unlock(1)\t response: {}\n", magic_enum::enum_name(client.unlock(1).first));
        fmt::print("unlock(1)\t response: {}\n", magic_enum::enum_name(client.unlock(1).first));
    } else {
        std::string command_format = "The Command Format is (op:lock|unlock) (object_id:uint32_t)";
        std::string command;
        std::cout << "Please Input Command:\t" << std::flush;
        while(std::getline(std::cin, command)) {
            std::vector<std::string> result;
            boost::split(result, command, boost::is_any_of(" "));
            if (result.size() != 2) { fmt::print("Invalid Command : {}\n", command_format); }
            std::string operation = result[0];
            std::string object_id_string = result[1];
            std::uint32_t object_id = 0;
            try {
                object_id = boost::lexical_cast<uint32_t>(object_id_string);
            } catch (boost::bad_lexical_cast& err) {
                fmt::print("Invalid Argument : {}\n", command_format);
            }
            boost::to_lower(operation);
            if (operation == std::string("lock")) {
                auto [response_op, response_object_id] = client.lock(object_id);
                fmt::print("lock({})\t response: {} on {}\n", object_id, magic_enum::enum_name(response_op), response_object_id);
            } else if (operation == std::string("unlock")) {
                auto [response_op, response_object_id] = client.unlock(object_id);
                fmt::print("unlock({})\t response: {} on {}\n", object_id, magic_enum::enum_name(response_op), response_object_id);
            } else {
                fmt::print("Invalid Operation : {}\n", command_format);
            }
            std::cout << "Please Input Command:\t" << std::flush;
        }
    }

    return 0;
}
