
#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <ctime>
#include <algorithm>
#include <climits>
#include <format>

/**
 * Struct to hold log file statistics sums for reduction.
 */
struct LogSums {
    // Total stats
    long total_requests;
    long total_bytes_sent;
    long total_response_time;

    // For calculating average response time
    long total_recorded_response_times;

    // Method totals
    long get_requests;
    long post_requests;
    long put_requests;
    long delete_requests;
    long patch_requests;
    long other_requests;

    // Individual status code totals
    long status_200;  // Success
    long status_301;  // Permanent redirect
    long status_304;  // Cache hit
    long status_400;  // Bad request
    long status_401;  // Auth required
    long status_403;  // Forbidden
    long status_404;  // Not found
    long status_500;  // Server error
    long status_502;  // Bad gateway
    long status_503;  // Service unavailable
    
    // Status category totals
    long status_2xx;  // All success
    long status_3xx;  // All redirects
    long status_4xx;  // All client errors
    long status_5xx;  // All server errors
};

/**
 * Struct to hold log file statistics minimums for reduction.
 */
struct LogMins {
    long min_bytes_sent;
    long min_response_time;
    long earliest_epoch;
};

/**
 * Struct to hold log file statistics maximums for reduction.
 */
struct LogMaxs {
    long max_bytes_sent;
    long max_response_time;
    long latest_epoch;
};

/**
 * Process a log line and updates local statistics variables.
 */
void process_log_line(
    const std::string& line,
    LogSums& local_sums,
    LogMins& local_mins,
    LogMaxs& local_maxs,
    std::unordered_map<std::string, int>& local_endpoint_counts,
    std::unordered_map<std::string, int>& local_ip_counts,
    std::unordered_map<std::string, int>& local_referrer_counts
) {
    if (line.empty()) return;

    std::istringstream iss(line);
    
    // Parse IP and update IP map
    std::string ip;
    iss >> ip;
    local_ip_counts[ip]++;

    // Parse identity and user (not handled)
    std::string ident, user;
    iss >> ident >> user;

    // Parse timestamp
    // Format: [10/Oct/2000:13:55:36 -0700]
    std::string timestamp;
    std::getline(iss, timestamp, '[');
    std::getline(iss, timestamp, ']');

    // Convert timestamp to epoch time and set min/max
    std::tm tm = {};
    strptime(timestamp.c_str(), "%d/%b/%Y:%H:%M:%S", &tm);
    time_t epoch_time = mktime(&tm);
    local_mins.earliest_epoch = std::min(local_mins.earliest_epoch, epoch_time);
    local_maxs.latest_epoch = std::max(local_maxs.latest_epoch, epoch_time);

    // Parse request string
    // Format: "GET /apache_pb.gif HTTP/1.0"
    std::string request;
    std::getline(iss, request, '"');
    std::getline(iss, request, '"');

    // Parse method, endpoint, and protocol from request string
    std::istringstream request_stream(request);
    std::string method, endpoint, protocol;
    request_stream >> method >> endpoint >> protocol;

    // Update method counts
    if (method == "GET") local_sums.get_requests++;
    else if (method == "POST") local_sums.post_requests++;
    else if (method == "PUT") local_sums.put_requests++;
    else if (method == "DELETE") local_sums.delete_requests++;
    else if (method == "PATCH") local_sums.patch_requests++;
    else local_sums.other_requests++;

    // Update endpoint map
    local_endpoint_counts[endpoint]++;

    // Parse status code
    int status_code;
    iss >> status_code;

    // Update individual status code counts
    switch (status_code) {
        case 200: local_sums.status_200++; break;
        case 303: local_sums.status_301++; break;
        case 304: local_sums.status_304++; break;
        case 400: local_sums.status_400++; break;
        case 401: local_sums.status_401++; break;
        case 403: local_sums.status_403++; break;
        case 404: local_sums.status_404++; break;
        case 500: local_sums.status_500++; break;
        case 502: local_sums.status_502++; break;
        case 503: local_sums.status_503++; break;
    }

    // Update status code category counts
    if (status_code >= 200 && status_code < 300) {
        local_sums.status_2xx++;
    } else if (status_code >= 300 && status_code < 400) {
        local_sums.status_3xx++;
    } else if (status_code >= 400 && status_code < 500) {
        local_sums.status_4xx++;
    } else if (status_code >= 500 && status_code < 600) {
        local_sums.status_5xx++;
    }

    // Parse bytes sent
    long bytes;
    iss >> bytes;
    local_sums.total_bytes_sent += bytes;

    // Increment total requests
    local_sums.total_requests++;

    // Check if there's anything left to read
    std::string remaining;
    std::getline(iss, remaining);

    // If remaining is empty or whitespace only, no response time
    remaining.erase(0, remaining.find_first_not_of(" \t\r\n"));  // Trim left
    if (remaining.empty()) {
        return;
    }

    // Parse referrer
    std::string referrer;
    std::getline(iss, referrer, '"');
    std::getline(iss, referrer, '"');
    if (!referrer.empty() && referrer != "-") {
        local_referrer_counts[referrer]++;
    }

    // Parse user agent (currently not handled)
    std::string user_agent;
    std::getline(iss, user_agent, '"');
    std::getline(iss, user_agent, '"');

    // Parse response time
    long response_time;
    if (!iss >> response_time) {
        // Not present, set to 0 and continue
        response_time = 0;
    } else {
        // Update total recorded response times for average calculation
        local_sums.total_recorded_response_times++;
    }

    // Update response time sums, mins, and maxs
    local_maxs.max_response_time = std::max(local_maxs.max_response_time, response_time);
    local_mins.min_response_time = std::min(local_mins.min_response_time, response_time);
    local_sums.total_response_time += response_time;
}

/**
 *  Serialize a map to a string for sending to rank 0 (format: "key1:value1;key2:value2;...")
 */
std::string serialize_map(
    const std::unordered_map<std::string, int>& map
) {
    std::stringstream ss;
    for (const auto& [key, value] : map) {
        if (!key.empty() && value > 0) {
            ss << key << "\t" << value << "\n";
        }
    }
    return ss.str();
}

/**
 *  Deserialize a string back into a map to be used in rank 0 results
 */
std::unordered_map<std::string, int> deserialize_map(
    const std::string& str
) {
    std::unordered_map<std::string, int> map;
    std::istringstream ss(str);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string key = line.substr(0, tab);
        int value = std::stoi(line.substr(tab + 1));
        map[key] += value;
    }
    return map;
}

/**
 * Print top n elements of a map in descending value order
 */
void print_map_top_n(const std::unordered_map<std::string, int>& map, int n = 5) {
    // Convert to vector
    std::vector<std::pair<std::string, int>> vec(map.begin(), map.end());
    
    // Sort by count (descending)
    std::sort(vec.begin(), vec.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Print top N
    for (size_t i = 0; i < std::min(vec.size(), (size_t)n); i++) {
        std::cout << "  " << vec[i].first << ": " << vec[i].second << std::endl;
    }
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double total_time_start = MPI_Wtime();

    if (argc < 2) {
        if (rank == 0) {
            std::cerr << "Usage: " << argv[0] << " <log_file>" << std::endl;
            std::cerr.flush();
        }
        MPI_Finalize();
        return 1;
    }

    std::string filename = argv[1];

    // =============================================================
    // PART 1: All ranks open file and read assigned chunk
    // - Open the log file using MPI I/O and get chunk size
    // - Read assigned chunk and convert to string
    // - Parse so that only complete lines remain 
    // =============================================================

    double file_io_time_start = MPI_Wtime();

    // Open file and get file size
    MPI_File fh;
    MPI_File_open(MPI_COMM_WORLD, filename.c_str(), MPI_MODE_RDONLY, MPI_INFO_NULL, &fh);

    MPI_Offset file_size;
    MPI_File_get_size(fh, &file_size);

    // Calulate chunk_size and boundaries for each rank
    MPI_Offset chunk_size = file_size / size;
    MPI_Offset start = rank * chunk_size;
    MPI_Offset end = (rank == size - 1) ? file_size : start + chunk_size;

    // Read chunk into buffer and convert to string for processing
    std::vector<char> read_buf(chunk_size);
    MPI_File_read_at_all(fh, start, read_buf.data(), chunk_size, MPI_CHAR, MPI_STATUS_IGNORE);
    
    // Parse buffer into lines
    std::string lines(read_buf.data(), read_buf.size());

    // Unless Rank 0, cut off string beginning until first newline
    if (rank != 0) {
        size_t first_newline = lines.find('\n');
        if (first_newline != std::string::npos) {
            lines = lines.substr(first_newline + 1);
        }
    }

    // Read in 1024 bytes and add to lines until reach next newline after end boundary
    // (Most lines will be much less than 1024 bytes, so in most cases this should only require one additional read.)
    if (rank != size - 1) {
        MPI_Offset LINE_FIND_SIZE = 1024;
        std::vector<char> line_read_buf(LINE_FIND_SIZE);
        bool found = false;
        MPI_Offset read_offset = end;

        while (!found && read_offset < file_size) {
            MPI_Offset bytes_to_read = std::min(LINE_FIND_SIZE, file_size - read_offset);

            MPI_Status status;
            MPI_File_read_at(fh, read_offset, line_read_buf.data(), bytes_to_read, MPI_CHAR, &status);

            int chars_read;
            MPI_Get_count(&status, MPI_CHAR, &chars_read);

            std::string line_section(line_read_buf.data(), chars_read);
            size_t first_newline = line_section.find('\n');

            // If no newline found, add entire section and read next chunk
            // If newline found, add up to newline and stop reading
            line_section = line_section.substr(0, first_newline);
            lines += line_section;
            if (first_newline != std::string::npos) {
                found = true;
            }

            read_offset += chars_read;
        }
    }

    double file_io_time_end = MPI_Wtime();
    double time_file_io = file_io_time_end - file_io_time_start;

    // =============================================================
    // PART 2: All ranks process lines into local statistics
    // - At this point, lines should contain only complete lines
    // - Loop through lines and parse to add to local stat variables
    // =============================================================

    double processing_time_start = MPI_Wtime();

    // Initialize local statistics variables
    LogSums local_sums = {};
    LogMins local_mins = {LONG_MAX, LONG_MAX, LONG_MAX};
    LogMaxs local_maxs = {};
    std::unordered_map<std::string, int> local_endpoint_counts = {};
    std::unordered_map<std::string, int> local_ip_counts = {};
    std::unordered_map<std::string, int> local_referrer_counts = {};

    // Initialize global statistics variables for reduction
    LogSums global_sums = {};
    LogMins global_mins = {};
    LogMaxs global_maxs = {};

    // Process lines and update local statistics
    std::istringstream line_stream(lines);
    std::string line;
    while (std::getline(line_stream, line)) {
        process_log_line(
            line,
            local_sums,
            local_mins,
            local_maxs,
            local_endpoint_counts,
            local_ip_counts,
            local_referrer_counts
        );
    }

    double processing_time_end = MPI_Wtime();
    double time_processing = processing_time_end - processing_time_start;

    // =============================================================
    // PART 3: All ranks reduce local statistics and send maps to rank 0
    // - Reduce sums, mins, and maxs to rank 0 using MPI_Reduce
    // - Serialize maps and send to rank 0 using MPI_Gather and MPI_Gatherv
    // =============================================================

    double communication_time_start = MPI_Wtime();

    // Do reductions for sums, mins, and maxs
    int sum_fields = sizeof(LogSums) / sizeof(long);
    MPI_Reduce(&local_sums, &global_sums, sum_fields, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    int min_fields = sizeof(LogMins) / sizeof(long);
    MPI_Reduce(&local_mins, &global_mins, min_fields, MPI_LONG, MPI_MIN, 0, MPI_COMM_WORLD);
    int max_fields = sizeof(LogMaxs) / sizeof(long);
    MPI_Reduce(&local_maxs, &global_maxs, max_fields, MPI_LONG, MPI_MAX, 0, MPI_COMM_WORLD);

    // Serialize maps and send to rank 0
    std::string serialized_endpoints = serialize_map(local_endpoint_counts);
    std::string serialized_ips = serialize_map(local_ip_counts);
    std::string serialized_referrers = serialize_map(local_referrer_counts);

    // Get serialized map sizes
    int local_endpoints_size = serialized_endpoints.size();
    int local_ips_size = serialized_ips.size();
    int local_referrers_size = serialized_referrers.size();

    // Global arrays to hold sizes for gathering at rank 0
    std::vector<int> all_endpoint_sizes(rank == 0 ? size : 0);
    std::vector<int> all_ip_sizes(rank == 0 ? size : 0);
    std::vector<int> all_referrer_sizes(rank == 0 ? size : 0);

    // Gather sizes at rank 0
    MPI_Gather(&local_endpoints_size, 1, MPI_INT, all_endpoint_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gather(&local_ips_size, 1, MPI_INT, all_ip_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gather(&local_referrers_size, 1, MPI_INT, all_referrer_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    // At rank 0, calculate displacements and total size for receiving serialized maps
    std::vector<int> endpoint_displs(rank == 0 ? size : 0);
    std::vector<int> ip_displs(rank == 0 ? size : 0);
    std::vector<int> referrer_displs(rank == 0 ? size : 0);
    int total_endpoint_size = 0;
    int total_ip_size = 0;
    int total_referrer_size = 0;
    if (rank == 0) {
        for (int i = 0; i < size; i++) {
            endpoint_displs[i] = total_endpoint_size;
            total_endpoint_size += all_endpoint_sizes[i];

            ip_displs[i] = total_ip_size;
            total_ip_size += all_ip_sizes[i];

            referrer_displs[i] = total_referrer_size;
            total_referrer_size += all_referrer_sizes[i];
        }
    }

    // Get all serialized maps at rank 0 as whole strings
    std::vector<char> all_serialized_endpoints(rank == 0 ? total_endpoint_size : 0);
    std::vector<char> all_serialized_ips(rank == 0 ? total_ip_size : 0);
    std::vector<char> all_serialized_referrers(rank == 0 ? total_referrer_size : 0);
    MPI_Gatherv(serialized_endpoints.data(), local_endpoints_size, MPI_CHAR, all_serialized_endpoints.data(), all_endpoint_sizes.data(), endpoint_displs.data(), MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Gatherv(serialized_ips.data(), local_ips_size, MPI_CHAR, all_serialized_ips.data(), all_ip_sizes.data(), ip_displs.data(), MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Gatherv(serialized_referrers.data(), local_referrers_size, MPI_CHAR, all_serialized_referrers.data(), all_referrer_sizes.data(), referrer_displs.data(), MPI_CHAR, 0, MPI_COMM_WORLD);

    double communication_time_end = MPI_Wtime();
    double time_communication = communication_time_end - communication_time_start;

    // =============================================================
    // PART 4: Rank 0 prints final statistics
    // - Calculates aggregate statistics from total values
    // - Prints all sums and aggregate statistics to console
    // =============================================================

    double results_time_start = MPI_Wtime();

    if (rank == 0) {
        // Deserialize maps back into global maps
        std::unordered_map<std::string, int> global_endpoint_counts = deserialize_map(std::string(all_serialized_endpoints.data(), total_endpoint_size));
        std::unordered_map<std::string, int> global_ip_counts = deserialize_map(std::string(all_serialized_ips.data(), total_ip_size));
        std::unordered_map<std::string, int> global_referrer_counts = deserialize_map(std::string(all_serialized_referrers.data(), total_referrer_size));

        // Get days between start and end times
        double seconds = std::difftime(global_maxs.latest_epoch, global_mins.earliest_epoch);
        double days = seconds / (60 * 60 * 24);

        // Convert max and min epoch times back to human-readable format
        std::tm* start_time_ptr = std::localtime(&global_mins.earliest_epoch);
        std::vector<char> start_buffer(100);
        std::strftime(start_buffer.data(), sizeof(start_buffer), "%Y-%m-%d %H:%M:%S", start_time_ptr);

        std::tm* end_time_ptr = std::localtime(&global_maxs.latest_epoch);
        std::vector<char> end_buffer(100);
        std::strftime(end_buffer.data(), sizeof(end_buffer), "%Y-%m-%d %H:%M:%S", end_time_ptr);

        // Print final statistics
        std::cout << "=========== LOG FILE ANALYTICS ===========" << std::endl;
        std::cout << start_buffer.data() << " to " << end_buffer.data() << std::endl;

        // Overall totals
        std::cout << "\n--- Overall Totals ---" << std::endl;
        std::cout << "Total Requests: " << global_sums.total_requests << std::endl;
        std::cout << "Total Bytes Sent: " << global_sums.total_bytes_sent << std::endl;
        std::cout << "Total Unique Endpoints: " << global_endpoint_counts.size() << std::endl;
        std::cout << "Total Unique IPs: " << global_ip_counts.size() << std::endl;

        // Top categories
        std::cout << "\n--- Top Categories ---" << std::endl;
        std::cout << "Top 5 Endpoints:" << std::endl;
        print_map_top_n(global_endpoint_counts, 5);
        std::cout << "Top 5 IPs:" << std::endl;
        print_map_top_n(global_ip_counts, 5);
        std::cout << "Top 5 Referrers:" << std::endl;
        print_map_top_n(global_referrer_counts, 5);

        // Get top 5 status codes
        std::vector<std::pair<int,long>> status_counts = {
            {200, global_sums.status_200},
            {301, global_sums.status_301},
            {304, global_sums.status_304},
            {400, global_sums.status_400},
            {401, global_sums.status_401},
            {403, global_sums.status_403},
            {404, global_sums.status_404},
            {500, global_sums.status_500},
            {502, global_sums.status_502},
            {503, global_sums.status_503}
        };
        std::sort(status_counts.begin(), status_counts.end(),
        [](const auto& a, const auto& b){
            return a.second > b.second;
        });
        std::cout << "Top 5 Status Codes:" << std::endl;
        for (int i = 0; i < 5 && i < status_counts.size(); i++) {
            std::cout << "  " << status_counts[i].first << ": " << status_counts[i].second << std::endl;
        }

        // Get top 5 methods
        std::vector<std::pair<std::string,long>> method_counts = {
            {"GET", global_sums.get_requests},
            {"POST", global_sums.post_requests},
            {"PUT", global_sums.put_requests},
            {"DELETE", global_sums.delete_requests},
            {"PATCH", global_sums.patch_requests},
            {"OTHER", global_sums.other_requests}
        };
        std::sort(method_counts.begin(), method_counts.end(),
        [](const auto& a, const auto& b){
            return a.second > b.second;
        });
        std::cout << "Top 5 Methods:" << std::endl;
        for (int i = 0; i < 5 && i < method_counts.size(); i++) {
            std::cout << "  " << method_counts[i].first << ": " << method_counts[i].second << std::endl;
        }

        // Averages
        std::cout << "\n--- Averages ---" << std::endl;
        if (global_sums.total_recorded_response_times > 0) {
            std::cout << "Response Time / Request: " << (double)global_sums.total_response_time / global_sums.total_recorded_response_times << std::endl;
        } else {
            std::cout << "Response Time / Request: N/A" << std::endl;
        }
        std::cout << "Bytes / Request: " << (double)global_sums.total_bytes_sent / global_sums.total_requests << std::endl;
        std::cout << "Requests / Day: " << global_sums.total_requests / days << std::endl;
        std::cout << "Unique IPs / Day: " << global_ip_counts.size() / days << std::endl;
        std::cout << "Requests / IP: " << global_sums.total_requests / global_ip_counts.size() << std::endl;

        // Request percentages
        std::cout << "\n--- Request Percentages ---" << std::endl;
        std::cout << "Status Codes:" << std::endl;
        std::cout << "  2XX: " << (double)global_sums.status_2xx / global_sums.total_requests * 100 << "%" << std::endl;
        std::cout << "  3XX: " << (double)global_sums.status_3xx / global_sums.total_requests * 100 << "%" << std::endl;
        std::cout << "  4XX: " << (double)global_sums.status_4xx / global_sums.total_requests * 100 << "%" << std::endl;
        std::cout << "  5XX: " << (double)global_sums.status_5xx / global_sums.total_requests * 100 << "%" << std::endl;
        std::cout << "Methods:" << std::endl;
        std::cout << "  GET: " << (double)global_sums.get_requests/ global_sums.total_requests * 100 << "%" << std::endl;
        std::cout << "  POST: " << (double)global_sums.post_requests / global_sums.total_requests * 100 << "%" << std::endl;
        std::cout << "  PUT: " << (double)global_sums.put_requests / global_sums.total_requests * 100 << "%" << std::endl;
        std::cout << "  DELETE: " << (double)global_sums.delete_requests / global_sums.total_requests * 100 << "%" << std::endl;
        std::cout << "  PATCH: " << (double)global_sums.patch_requests / global_sums.total_requests * 100 << "%" << std::endl;
        std::cout << "  OTHER: " << (double)global_sums.other_requests / global_sums.total_requests * 100 << "%" << std::endl;
    }

    double results_time_end = MPI_Wtime();
    double time_results = results_time_end - results_time_start;

    double total_end_time = MPI_Wtime();
    double time_total = total_end_time - total_time_start;

    // =============================================================
    // PART 4: Get timing stats and print to console
    // - Get total rank 0 time
    // - Get min, max, and average times for file I/O, processing, 
    //   communication, and results across all ranks
    // =============================================================

    // Gather timing stats from all ranks
    double max_communication_time, min_communication_time, avg_communication_time;
    MPI_Reduce(&time_communication, &max_communication_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time_communication, &min_communication_time, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time_communication, &avg_communication_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        avg_communication_time /= size;
    }

    double max_processing_time, min_processing_time, avg_processing_time;
    MPI_Reduce(&time_processing, &max_processing_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time_processing, &min_processing_time, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time_processing, &avg_processing_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        avg_processing_time /= size;
    }

    double max_results_time, min_results_time, avg_results_time;
    MPI_Reduce(&time_results, &max_results_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time_results, &min_results_time, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time_results, &avg_results_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        avg_results_time /= size;
    }

    double max_file_io_time, min_file_io_time, avg_file_io_time;
    MPI_Reduce(&time_file_io, &max_file_io_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time_file_io, &min_file_io_time, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&time_file_io, &avg_file_io_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        avg_file_io_time /= size;
    }

    // Print timing stats at rank 0
    if (rank == 0) {
        std::cout << "\n=========== TIMING ===========" << std::endl;

        std::cout << "\nRank 0 Total Time: " << time_total << " seconds" << std::endl;

        double sum_of_maxes = max_file_io_time + max_processing_time + max_communication_time + max_results_time;
        std::cout << "Max Times Sum: " << sum_of_maxes << " seconds" << std::endl << std::endl;

        std::cout << std::format("{:<16} {:<14} {:<14} {:<14} {:<14}\n",
            "Phase", "Min Time (s)", "Avg Time (s)", "Max Time (s)", "% of Max Total");
        std::cout << std::format("{:<16} {:<14.6f} {:<14.6f} {:<14.6f} {:.2f}%\n",
            "File I/O", min_file_io_time, avg_file_io_time, max_file_io_time,
            max_file_io_time / sum_of_maxes * 100);
        std::cout << std::format("{:<16} {:<14.6f} {:<14.6f} {:<14.6f} {:.2f}%\n",
            "Processing", min_processing_time, avg_processing_time, max_processing_time,
            max_processing_time / sum_of_maxes * 100);
        std::cout << std::format("{:<16} {:<14.6f} {:<14.6f} {:<14.6f} {:.2f}%\n",
            "Communication", min_communication_time, avg_communication_time, max_communication_time,
            max_communication_time / sum_of_maxes * 100);
        std::cout << std::format("{:<16} {:<14.6f} {:<14.6f} {:<14.6f} {:.2f}%\n",
            "Results", min_results_time, avg_results_time, max_results_time,
            max_results_time / sum_of_maxes * 100);
    }

    MPI_File_close(&fh);
    MPI_Finalize();
    return 0;
}

/**
mpic++ -std=c++20 -o log_processor.bin log_processor.cpp
mpirun -np 8 ./log_processor.bin
*/