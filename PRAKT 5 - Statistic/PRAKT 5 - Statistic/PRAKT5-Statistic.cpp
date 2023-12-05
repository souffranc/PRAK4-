#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>

using namespace boost::asio;
using ip::tcp;
using namespace std;
using json = nlohmann::json;



json GenerateReport(const string& selectedParameter) {
    json report;

    // Открываем файл JSON
    ifstream json_file("statistics.json");

    if (json_file.is_open()) {
        // Читаем весь файл в JSON объект
        json data;
        json_file >> data;

        // Закрываем файл
        json_file.close();

        int idCounter = 0;


        // Обрабатываем каждый объект в массиве
        for (const auto& entry : data) {
            // Логика формирования отчета с учетом выбранного параметра
            json reportEntry;
            reportEntry["Id"] = idCounter++;
            reportEntry["ShortURL"] = nullptr;
            reportEntry["SourceIP"] = nullptr;
            reportEntry["TimeInterval"] = nullptr;
            reportEntry["URL"] = nullptr;

            // В зависимости от порядкового номера записи задаем значения полей
            if (idCounter%3 == 1) {
                if (entry.contains("URL")) {
                    reportEntry["URL"] = entry["URL"];
                }
            }
            else if (idCounter%3 == 2) {
                if (entry.contains("SourceIP")) {
                    reportEntry["SourceIP"] = entry["SourceIP"];
                }
            }
            else if (idCounter%3 == 0) {
                if (entry.contains("TimeInterval")) {
                    reportEntry["TimeInterval"] = entry["TimeInterval"];
                }
            }

            // Добавляем запись в отчет, если хотя бы одно поле не null
            if (reportEntry["URL"] != nullptr || reportEntry["SourceIP"] != nullptr || reportEntry["TimeInterval"] != nullptr) {
                report.push_back(reportEntry);
            }
        }

        return report;
    }
}




void handle_request(tcp::socket socket) {
    boost::asio::streambuf request_buffer;
    boost::system::error_code ignored_error;

    // Чтение HTTP-запроса
    boost::asio::read_until(socket, request_buffer, "\r\n\r\n");
    istream request_stream(&request_buffer);
    string request_line;
    getline(request_stream, request_line);

    // Обработка только POST-запросов
    if (request_line.find("POST /stat") != string::npos) {
        // Чтение тела запроса
        stringstream body_stream;
        body_stream << request_stream.rdbuf();
        string body = body_stream.str();

        // Парсинг тела запроса
        string ip, url, time, shortUrl;
        size_t ip_pos = body.find("ip=");
        size_t url_pos = body.find("&url=");
        size_t shortUrl_pos = body.find("&shortUrl=");
        size_t time_pos = body.find("&time=");

        if (ip_pos != string::npos && url_pos != string::npos && shortUrl_pos != string::npos && time_pos != string::npos) {
            ip = body.substr(ip_pos + 3, url_pos - ip_pos - 3);
            url = body.substr(url_pos + 5, time_pos - url_pos - 5);
            time = body.substr(time_pos + 6, shortUrl_pos - time_pos - 6);
            shortUrl = body.substr(shortUrl_pos + 10);

        }

        // Открываем файл JSON
        ifstream json_file("statistics.json");
        json data;
        if (json_file.is_open()) {
            // Читаем весь файл в JSON объект
            json_file >> data;

            // Закрываем файл
            json_file.close();
        }
        // Создаем новый JSON объект для текущего запроса
        json jsonObject;
        jsonObject["ShortURL"] = shortUrl;
        jsonObject["URL"] = url;
        jsonObject["SourceIP"] = ip;
        jsonObject["TimeInterval"] = time.substr(0, time.find('\n'));
        // Добавляем новый JSON объект в массив
        data.push_back(jsonObject);
        // Записываем обновленные данные в файл
        ofstream json_outfile("statistics.json");
        if (json_outfile.is_open()) {
            json_outfile << data.dump(4) << std::endl;
            json_outfile.close();
        }
        else {
            cerr << "Error opening JSON file for writing" << endl;
        }
    }
    else if (request_line.find("GET /REPORT") != string::npos) {
        // Обработка GET-запроса на получение отчета

        size_t params_start = request_line.find("?");
        if (params_start != string::npos) {
            // Выделяем подстроку, начиная с символа "?" до конца строки
            string params = request_line.substr(params_start + 1);

            // Ищем первую пустоту в строке params
            size_t first_space_pos = params.find(" ");

            // Если нашли пробел, обрезаем строку params до этого места
            if (first_space_pos != string::npos) {
                params = params.substr(0, first_space_pos);
            }

            // Теперь params содержит только параметры после знака "="
            size_t equal_sign_pos = params.find("=");
            if (equal_sign_pos != string::npos && equal_sign_pos < params.length() - 1) {
                // Извлекаем значение после знака "="
                string selectedParameter = params.substr(equal_sign_pos + 1);

                // Теперь selectedParameter содержит "SourceIP" из вашего примера
                json result = GenerateReport(selectedParameter);

                // Выводим результат
                cout << result.dump(3) << endl;
            }
           
        }
    }
}
      
int main() {
    io_service io;
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8081));

    cout << "Statistics server is running on port 8081. Waiting for incoming connections..." << endl;

    while (true) {
        tcp::socket socket(io);
        acceptor.accept(socket);

        
        thread(handle_request, move(socket)).detach();
    }

    return 0;
}