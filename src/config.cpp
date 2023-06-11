//
// Created by Jiacheng Wu on 11/12/22.
//

#include <fstream>
#include "config.hpp"
#include "json.hpp"
#include "magic_enum.hpp"

static boost::log::trivial::severity_level get_boost_log_level_from_str(std::string &log_level) {
    if (log_level == std::string("trace")) {
        return boost::log::trivial::trace;
    } else if (log_level == std::string("debug")) {
        return boost::log::trivial::debug;
    } else if (log_level == std::string("info")) {
        return boost::log::trivial::info;
    } else if (log_level == std::string("warning")) {
        return boost::log::trivial::warning;
    } else if (log_level == std::string("error")) {
        return boost::log::trivial::error;
    } else if (log_level == std::string("fatal")) {
        return boost::log::trivial::fatal;
    } else {
        BOOST_LOG_TRIVIAL(error) << std::format("Not Valid log_level {}, Use info!!!\n",
                                                log_level);
        return boost::log::trivial::info;
    }
}

bool Config::load_config(std::filesystem::path json_filepath) {
    if (!std::filesystem::exists(json_filepath)) {
        BOOST_LOG_TRIVIAL(warning) << std::format("Cannot Find Config file {}, Use Default Config!!!\n",
                                                  json_filepath.generic_string());
        load_config();
        return false;
    }
    config_file_name = json_filepath.generic_string();
    std::ifstream f(json_filepath);
    nlohmann::json data = nlohmann::json::parse(f);
    if (data.contains("log_level")) {
        std::string log_level_config = data["log_level"];
        this->log_level = get_boost_log_level_from_str(log_level_config);
    } else {
        BOOST_LOG_TRIVIAL(error) << std::format("{} do not contain log_level, Use info!!!\n",
                                                json_filepath.generic_string());
    }
    this->after_prepare_milliseconds = data.at("after_prepare_milliseconds").get<std::uint32_t>();
    this->after_accept_milliseconds = data.at("after_accept_milliseconds").get<std::uint32_t>();
    this->client_retry_milliseconds = data.at("client_retry_milliseconds").get<std::uint32_t>();
    this->network_send_retry_times = data.at("network_send_retry_times").get<std::uint32_t>();
    this->need_recovery = data.at("need_recovery").get<bool>();
    this->at_most_once = data.at("at_most_once").get<bool>();
    if (data.contains("id_addr_map") && data["id_addr_map"].is_array()) {
        nlohmann::json id_addr_map_config = data["id_addr_map"];
        for (auto &id_addr: id_addr_map_config) {
            std::uint32_t id = id_addr.at("id").get<std::uint32_t>();
            std::string ip = id_addr.at("ip").get<std::string>();
            std::uint32_t port = id_addr.at("port").get<std::uint32_t>();
            server_id_to_addr_map[id] = server_addr_t(boost::asio::ip::make_address(ip), port);
        }
    } else {
        BOOST_LOG_TRIVIAL(error) << std::format("{} do not contain id address map array !!!, cannot execute then\n",
                                                json_filepath.generic_string());
        exit(1);
    }
    number_of_nodes = server_id_to_addr_map.size();

    return true;
}

bool Config::load_config() {
    server_id_to_addr_map = {
            {0, server_addr_t(boost::asio::ip::udp::v4(), 8080)},
            {1, server_addr_t(boost::asio::ip::udp::v4(), 8081)},
            {2, server_addr_t(boost::asio::ip::udp::v4(), 8082)},
//            {3, server_addr_t(boost::asio::ip::udp::v4(), 8083)},
//            {4, server_addr_t(boost::asio::ip::udp::v4(), 8084)},
    };
    number_of_nodes = server_id_to_addr_map.size();
    return true;
}

std::filesystem::path Config::get_learner_file_path(Config::server_id_t server_id) {
    return std::filesystem::path(std::format("./server{}_learner.log", server_id));
}

std::filesystem::path Config::get_acceptor_file_path(Config::server_id_t server_id) {
    return std::filesystem::path(std::format("./server{}_acceptor.log", server_id));
}

void Config::log_detail_infos() const {
    BOOST_LOG_TRIVIAL(debug) << std::format("Use config file: {}", config_file_name != "" ? config_file_name : "none (use hard coded default)");
    BOOST_LOG_TRIVIAL(debug) << std::format("  {} : {}, ", "log_level", magic_enum::enum_name(log_level));
    BOOST_LOG_TRIVIAL(debug) << std::format("  {} : {}, ", "after_prepare_milliseconds", after_prepare_milliseconds);
    BOOST_LOG_TRIVIAL(debug) << std::format("  {} : {}, ", "after_accept_milliseconds", after_accept_milliseconds);
    BOOST_LOG_TRIVIAL(debug) << std::format("  {} : {}, ", "client_retry_milliseconds", client_retry_milliseconds);
    BOOST_LOG_TRIVIAL(debug) << std::format("  {} : {}, ", "network_send_retry_times", network_send_retry_times);
    BOOST_LOG_TRIVIAL(debug) << std::format("  {} : {} -- ", "server_id_to_addr_map", server_id_to_addr_map.size());
    for (auto &&[id, addr]: server_id_to_addr_map) {
        BOOST_LOG_TRIVIAL(debug) << std::format("    id : {} , addr: {}:{} ",
                                                id, addr.address().to_string(), addr.port());
    }
}

std::filesystem::path Config::get_client_file_path(std::uint16_t port) {
    return std::filesystem::path(std::format("./client{}.log", port));
}
