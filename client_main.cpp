//
// Created by Jiacheng Wu on 10/6/22.
//

#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/lexical_cast.hpp>

#include "magic_enum.hpp"
#include "client.hpp"

void init_log_level(boost::log::trivial::severity_level log_level) {
    boost::log::core::get()->set_filter
            (
                    boost::log::trivial::severity >= log_level
            );
}

std::tuple<std::uint16_t, std::string, std::string> parse_argument(int argc, char *argv[]) {
    std::uint16_t port_number = 0;
    std::string config_filepath_str = "../config.json";
    std::string test_filepath_str = "";
    if (argc == 1 || argc >= 5) {
        fmt::print("{} {} {} {}\n", "program_name",
                   "(port = 0)", "(config path)", "(testfile path) or interactive");
        exit(0);
    } else {
        try {
            port_number = boost::lexical_cast<uint16_t>(argv[1]);
            if (!is_registered_port(port_number)) {
                BOOST_LOG_TRIVIAL(warning)
                    << fmt::format("Port {} is not a registered port in [1024, 49151], set to 0 then!\n", port_number);
                port_number = 0;
            }
        } catch (boost::bad_lexical_cast &err) {
            fmt::print("{} {} {} {}\n", "program_name",
                       "(port = 0)", "(config path)", "(testfile path) or interactive");
            exit(0);
        }

        if (argc >= 3) {
            config_filepath_str = std::string(argv[2]);
        }

        if (argc >= 4) {
            test_filepath_str = std::string(argv[3]);
        }
    }
    return {port_number, config_filepath_str, test_filepath_str};
}

int main(int argc, char *argv[]) {

    auto [port_number, config_filepath_str, test_filepath_str] = parse_argument(argc, argv);

    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->load_config(std::filesystem::path(config_filepath_str));

    init_log_level(config->log_level);

    config->log_detail_infos();

    if (config->at_most_once && port_number == 0) {
        BOOST_LOG_TRIVIAL(warning)
            << fmt::format("Not registered port may not support at-most once semantics!\n", port_number);
        config->at_most_once = false;
    }

    PaxosClient client(std::move(config), port_number);
    std::istream *input_stream = &std::cin;
    std::ifstream fin;

    if (test_filepath_str != std::string("")) {
        std::filesystem::path test_filepath = std::filesystem::path(test_filepath_str);
        if (!std::filesystem::exists(test_filepath)) {
            BOOST_LOG_TRIVIAL(warning) << fmt::format("Cannot Find test file {}, Become Interative!!!\n",
                                                      test_filepath.generic_string());
        } else {
            fin.open(test_filepath);
            input_stream = &fin;
        }
    }


    std::string command_format = "The Command Format is [(op:lock|unlock) (object_id:uint32_t)] or [wait 1000] ms ";
    std::string command;
    if (input_stream == &std::cin) {
        std::cout << "Please Input Command:\t" << std::flush;
    }
    while (std::getline(*input_stream, command)) {
        boost::algorithm::trim(command);
        if (command.empty()) {
            continue;
        }
        std::vector<std::string> result;
        boost::split(result, command, boost::is_any_of(" "));
        result.erase(std::remove_if(result.begin(),
                                    result.end(),
                                    [](const std::string &x) { return x.empty(); }),
                     result.end());
        if (result.size() != 2) { fmt::print("Invalid Command : {}\n", command_format); }
        std::string operation = result[0];
        std::string object_id_string = result[1];
        std::uint32_t object_id = 0;
        try {
            object_id = boost::lexical_cast<uint32_t>(object_id_string);
        } catch (boost::bad_lexical_cast &err) {
            fmt::print("Invalid Argument : {}\n", command_format);
        }
        boost::to_lower(operation);
        if (operation == std::string("lock")) {
            auto [response_op, response_object_id] = client.lock(object_id);
            fmt::print("lock({})   response:  {:<15} on object {}\n", object_id, magic_enum::enum_name(response_op),
                       response_object_id);
        } else if (operation == std::string("unlock")) {
            auto [response_op, response_object_id] = client.unlock(object_id);
            fmt::print("unlock({}) response:  {:<15} on object {}\n", object_id, magic_enum::enum_name(response_op),
                       response_object_id);
        } else if (operation == std::string("wait")) {
            fmt::print("wait({}ms) response:  none\n", object_id);
            std::this_thread::sleep_for(std::chrono::milliseconds(object_id));
        } else {
            fmt::print("Invalid Operation : {}\n", command_format);
        }
        if (input_stream == &std::cin) {
            std::cout << "Please Input Command:\t" << std::flush;
        }
    }

    if (fin.is_open()) {
        fin.close();
    }

    return 0;
}
