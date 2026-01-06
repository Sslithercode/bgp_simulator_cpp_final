#include <iostream>
#include <fstream>
#include <string>
#include <curl/curl.h>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>

class CAIDADownloader {
private:
    std::string base_url = "https://publicdata.caida.org/datasets/as-relationships/serial-2/";
    std::string output_filename;
    std::string cache_metadata_file;

    // Callback function for writing data received from curl
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        std::ofstream* out = static_cast<std::ofstream*>(userp);
        out->write(static_cast<char*>(contents), total_size);
        return total_size;
    }

    // Callback for reading headers
    static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
        size_t total_size = size * nitems;
        std::string* headers = static_cast<std::string*>(userdata);
        headers->append(buffer, total_size);
        return total_size;
    }

    // Get the previous month's year and month in YYYYMM format
    std::string getPreviousMonthString() {
        time_t now = time(nullptr);
        tm* current_time = localtime(&now);

        // Go back one month
        current_time->tm_mon -= 1;
        if (current_time->tm_mon < 0) {
            current_time->tm_mon = 11;
            current_time->tm_year -= 1;
        }

        mktime(current_time);  // Normalize the time structure

        std::ostringstream oss;
        oss << std::setfill('0')
            << (current_time->tm_year + 1900)
            << std::setw(2) << (current_time->tm_mon + 1);
        return oss.str();
    }

    // Get file size in bytes
    long getFileSize(const std::string& filename) {
        struct stat file_stat;
        if (stat(filename.c_str(), &file_stat) == 0) {
            return file_stat.st_size;
        }
        return -1;
    }

    // Get file modification time
    time_t getFileModTime(const std::string& filename) {
        struct stat file_stat;
        if (stat(filename.c_str(), &file_stat) == 0) {
            return file_stat.st_mtime;
        }
        return 0;
    }

    // Check if file already exists and is recent (from current month's data)
    bool isFileRecentlyDownloaded(const std::string& filename) {
        struct stat file_stat;
        if (stat(filename.c_str(), &file_stat) != 0) {
            return false;  // File doesn't exist
        }

        // Check if file has content
        if (file_stat.st_size == 0) {
            return false;
        }

        time_t now = time(nullptr);
        tm* current_time = localtime(&now);

        // Get file modification time
        tm* file_time = localtime(&file_stat.st_mtime);

        // Check if file is from the same month OR from last month
        // (Since we download previous month's data, it should be valid for current month)
        if (file_time->tm_year == current_time->tm_year) {
            // Same year - check if from current or previous month
            if (file_time->tm_mon == current_time->tm_mon ||
                file_time->tm_mon == current_time->tm_mon - 1) {
                return true;
            }
        } else if (file_time->tm_year == current_time->tm_year - 1) {
            // Previous year - only valid if it's December and we're in January
            if (file_time->tm_mon == 11 && current_time->tm_mon == 0) {
                return true;
            }
        }

        return false;
    }

    // Save cache metadata for optimization
    void saveCacheMetadata(const std::string& etag, const std::string& last_modified) {
        std::ofstream meta(cache_metadata_file);
        if (meta.is_open()) {
            meta << "ETag: " << etag << "\n";
            meta << "Last-Modified: " << last_modified << "\n";
            meta.close();
        }
    }

    // Load cache metadata
    bool loadCacheMetadata(std::string& etag, std::string& last_modified) {
        std::ifstream meta(cache_metadata_file);
        if (!meta.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(meta, line)) {
            if (line.find("ETag: ") == 0) {
                etag = line.substr(6);
            } else if (line.find("Last-Modified: ") == 0) {
                last_modified = line.substr(15);
            }
        }
        meta.close();
        return !etag.empty() || !last_modified.empty();
    }

    // Try to download with fallback to earlier months
    bool tryDownloadWithFallback(const std::string& base_filename, int months_to_try = 6) {
        time_t now = time(nullptr);
        tm* current_time = localtime(&now);

        for (int i = 1; i <= months_to_try; i++) {
            tm try_time = *current_time;
            try_time.tm_mon -= i;
            if (try_time.tm_mon < 0) {
                try_time.tm_mon += 12;
                try_time.tm_year -= 1;
            }
            mktime(&try_time);

            std::ostringstream oss;
            oss << std::setfill('0')
                << (try_time.tm_year + 1900)
                << std::setw(2) << (try_time.tm_mon + 1)
                << "01." << base_filename;

            std::string filename = oss.str();
            std::string full_url = base_url + filename;

            std::cout << "Trying: " << full_url << std::endl;

            if (attemptDownload(full_url, filename)) {
                return true;
            }

            std::cout << "Not available, trying earlier month..." << std::endl;
        }

        return false;
    }

    // Attempt to download from a specific URL
    bool attemptDownload(const std::string& full_url, const std::string& remote_filename) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Error: Failed to initialize CURL" << std::endl;
            return false;
        }

        // Quick HEAD request to check availability
        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_reset(curl);

        if (res != CURLE_OK || http_code != 200) {
            curl_easy_cleanup(curl);
            return false;
        }

        // File is available, proceed with download
        std::string compressed_filename = remote_filename;

        std::ofstream output_file(compressed_filename, std::ios::binary);
        if (!output_file.is_open()) {
            std::cerr << "Error: Could not open output file" << std::endl;
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output_file);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L);

        std::cout << "Downloading..." << std::endl;
        res = curl_easy_perform(curl);
        output_file.close();

        if (res != CURLE_OK) {
            std::cerr << "Error: Download failed - " << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl);
            remove(compressed_filename.c_str());
            return false;
        }

        curl_easy_cleanup(curl);

        // Decompress
        std::cout << "Decompressing..." << std::endl;
        std::string decompress_cmd = "bzip2 -d -f " + compressed_filename;
        if (system(decompress_cmd.c_str()) != 0) {
            std::cerr << "Error: Failed to decompress" << std::endl;
            return false;
        }

        // Rename to standard filename
        std::string decompressed = compressed_filename.substr(0, compressed_filename.length() - 4);
        if (rename(decompressed.c_str(), output_filename.c_str()) != 0) {
            // If rename fails, file might already have correct name
            if (decompressed != output_filename) {
                std::cerr << "Warning: Could not rename file" << std::endl;
            }
        }

        std::cout << "Success! File: " << output_filename
                  << " (" << getFileSize(output_filename) << " bytes)" << std::endl;
        return true;
    }

public:
    CAIDADownloader() {
        output_filename = "as-rel.txt";
        cache_metadata_file = ".caida_cache_metadata";
    }

    // Download the CAIDA AS relationship file with optimized caching
    bool downloadASRelationships() {
        // OPTIMIZATION 1: Check if we already have a recent download based on month
        if (isFileRecentlyDownloaded(output_filename)) {
            long file_size = getFileSize(output_filename);
            std::cout << "[CACHE HIT] File " << output_filename
                      << " is up-to-date (size: " << file_size << " bytes). Skipping download."
                      << std::endl;
            return true;
        }

        std::cout << "Searching for latest available CAIDA AS relationship data..." << std::endl;

        // Try to download with fallback (tries up to 6 months back)
        // Note: CAIDA filenames are "as-rel2.txt" not "as-rel.txt"
        if (tryDownloadWithFallback("as-rel2.txt.bz2", 6)) {
            return true;
        }

        std::cerr << "Error: Could not find any available CAIDA data in the last 6 months" << std::endl;
        return false;
    }

    std::string getOutputFilename() const {
        return output_filename;
    }
};

int main() {
    std::cout << "CAIDA AS Relationship Downloader" << std::endl;
    std::cout << "=================================" << std::endl;

    CAIDADownloader downloader;

    if (!downloader.downloadASRelationships()) {
        std::cerr << "Failed to download AS relationships" << std::endl;
        return 1;
    }

    std::cout << "Download complete. File available at: "
              << downloader.getOutputFilename() << std::endl;

    return 0;
}
