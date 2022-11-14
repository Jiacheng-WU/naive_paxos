//
// Created by Jiacheng Wu on 10/6/22.
//

#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/lexical_cast.hpp>
#include "server.hpp"

int main(int argc, char* argv[]) {
//    auto k = std::move(do_nothing_handler);
//    do_nothing_handler(nullptr, nullptr, {});


    if (argc == 1 || argc >= 4) {
        std::cout << fmt::format("{} {} {}\n", "program_name", "id", "(recovery)");
        return 0;
    }

    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->load_config();

    std::uint32_t current_id = 0;
    try {
        current_id = boost::lexical_cast<uint32_t>(argv[1]);
        assert(current_id < config->number_of_nodes);
    } catch (boost::bad_lexical_cast& err) {
        std::cout << fmt::format("{} {}\n", "program_name", "id");
        return 0;
    }

    bool need_recovery = false;
    if (argc == 3 && std::string(argv[2]) == "recovery") {
        need_recovery = true;
    }

    BOOST_LOG_TRIVIAL(debug) << fmt::format("Server {} : {}\n", current_id, need_recovery? "recovery": "no recovery");

    boost::asio::io_context io_context;

    PaxosServer server(io_context, current_id, std::move(config));
    if (need_recovery) {
        server.recover();
    }


    boost::asio::signal_set signals(io_context, SIGINT | SIGTERM);
    signals.async_wait([&server, &io_context](const boost::system::error_code& error, int signal_number ) {
        server.stop();
        io_context.stop();
        BOOST_LOG_TRIVIAL(debug) << fmt::format("Server {} : stop\n", server.get_id());
        // exit(1);
    });


    server.start();
    BOOST_LOG_TRIVIAL(debug) << fmt::format("Server {} : start\n", server.get_id());
    io_context.run();

    std::cout << sizeof(boost::asio::ip::udp::endpoint) << std::endl;
    return 0;
}