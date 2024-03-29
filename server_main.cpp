//
// Created by Jiacheng Wu on 10/6/22.
//

#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include "server.hpp"

void init_log_level(boost::log::trivial::severity_level log_level) {
    boost::log::core::get()->set_filter
            (
                    boost::log::trivial::severity >= log_level
            );
}

int main(int argc, char *argv[]) {
//    auto k = std::move(do_nothing_handler);
//    do_nothing_handler(nullptr, nullptr, {});


    if (argc == 1 || argc >= 4) {
        std::cout << std::format("{} {} {}\n", "program_name", "id", "(config path)");
        return 0;
    }

    std::string config_filepath_str = "../config.json";
    if (argc == 3) {
        config_filepath_str = std::string(argv[2]);
    }

    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->load_config(std::filesystem::path(config_filepath_str));

    init_log_level(config->log_level);

    config->log_detail_infos();

    std::uint32_t current_id = 0;
    try {
        current_id = boost::lexical_cast<uint32_t>(argv[1]);
        if(current_id >= config->number_of_nodes) {
            std::cout << std::format("The server id {} should be less than #nodes {}\n", current_id, config->number_of_nodes);
            return 0;
        }
    } catch (boost::bad_lexical_cast &err) {
        std::cout << std::format("{} {} {}\n", "program_name", "id", "(config path)");
        return 0;
    }

    BOOST_LOG_TRIVIAL(debug) << std::format("Server {} : {}\n", current_id, config->need_recovery ? "recovery" : "no recovery");

    boost::asio::io_context io_context;

    PaxosServer server(io_context, current_id, std::move(config));

    boost::asio::signal_set signals(io_context, SIGINT | SIGTERM);
    signals.async_wait([&server, &io_context](const boost::system::error_code &error, int signal_number) {
        server.stop();
        io_context.stop();
        BOOST_LOG_TRIVIAL(debug) << std::format("Server {} : stop\n", server.get_id());
        // exit(1);
    });


    server.start();
    BOOST_LOG_TRIVIAL(debug) << std::format("Server {} : start\n", server.get_id());
    io_context.run();

    // std::cout << sizeof(boost::asio::ip::udp::endpoint) << std::endl;
    return 0;
}