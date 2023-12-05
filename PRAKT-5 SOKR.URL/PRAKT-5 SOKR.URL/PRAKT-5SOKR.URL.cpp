
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
    #include <string>
    #include <unordered_map>
    #include <boost/asio.hpp>
    #include <boost/algorithm/string.hpp>
    #include <boost/lexical_cast.hpp>
    #include <random>
    #include <fstream>
    #include <chrono>
    #include <ctime>



    using namespace boost::asio;
    using namespace std;
    using ip::tcp;


    string url_decode(const string& input) {
        string decoded;
        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '%' && i + 2 < input.size()) {
                // Если встречается '%', декодируем следующие два символа
                char hex[3] = { input[i + 1], input[i + 2], '\0' };
                int value;
                if (sscanf_s(hex, "%x", &value) == 1) {
                    decoded.push_back(static_cast<char>(value));
                    i += 2;
                }
                else {
                    // Некорректная последовательность, оставляем '%'
                    decoded.push_back('%');
                }
            }
            else if (input[i] == '+') {
                // Заменяем '+' на пробел
                decoded.push_back(' ');
            }
            else {
                // Остальные символы копируем без изменений
                decoded.push_back(input[i]);
            }
        }
        return decoded;
    }

    string SendShortenRequestToDatabase(const string& longUrl) {
        io_service io;
        tcp::resolver resolver(io);
        tcp::socket socket(io);

        connect(socket, resolver.resolve({ "localhost", "8082" }));

        // Формируем текстовый запрос для сокращения URL
        string request_data = "action=shorten&longUrl=" + longUrl;

        // Отправляем HTTP-запрос на сервер базы данных
        boost::asio::streambuf request_buffer;
        std::ostream request_stream(&request_buffer);
        request_stream << "POST /shorten HTTP/1.1\r\n";
        request_stream << "Host: localhost:8082\r\n";
        request_stream << "Content-Type: application/x-www-form-urlencoded\r\n";  
        request_stream << "Content-Length: " << request_data.length() << "\r\n";
        request_stream << "\r\n";
        request_stream << request_data;

        write(socket, request_buffer);

        boost::asio::streambuf response_buffer;
        boost::asio::read_until(socket, response_buffer, "\r\n\r\n");

        // Просто используйте std::istream без создания дополнительного streambuf
        std::istream response_stream(&response_buffer);

        // Парсим ответ сервера (предполагаем, что ответ в формате текста)
        string http_version;
        response_stream >> http_version;
        unsigned int status_code;
        response_stream >> status_code;

        string status_message;
        getline(response_stream, status_message);

        if (status_code == 200) {
            // Успешный ответ, читаем дополнительные данные
            read_until(socket, response_buffer, "\r\n\r\n");
            string dummy_line;
            getline(response_stream, dummy_line);
            getline(response_stream, dummy_line);

            string response_data;
            getline(response_stream, response_data);
            return response_data;
        }
        else {
            // Ошибка в ответе
            cerr << "Error: " << status_code << " " << status_message << endl;
            return "";
        }
    }
    

    string ShortenUrl(const string& longUrl) {
        // Отправляем запрос на сервер базы данных
        string response_data = SendShortenRequestToDatabase(longUrl);

        // Обрабатываем ответ сервера 
        if (!response_data.empty()) {
            return response_data;
        }
        else {
            // В случае ошибки или пустого ответа возвращаем пустую строку
            return "";
        }
    }

    void SendToStatisticServer(const string& ip, const string& url, const string& time, const string& shortUrl) {
        io_service io;
        tcp::socket socket(io);
        tcp::resolver resolver(io);
        connect(socket, resolver.resolve({ "localhost", "8081" }));

        // Формируем JSON-объект
        // Формируем данные для отправки
        string data = "ip=" + ip + "&url=" + url + "&time=" + time + "&shortUrl=" + shortUrl;

        // Отправляем HTTP-запрос на сервер статистики
        stringstream request_stream;
        request_stream << "POST /stat HTTP/1.1\r\n";
        request_stream << "Host: localhost:8081\r\n";
        request_stream << "Content-Type: application/x-www-form-urlencoded\r\n";
        request_stream << "Content-Length: " << data.length() << "\r\n";
        request_stream << "\r\n";
        request_stream << data;

        write(socket, buffer(request_stream.str()));


      
                 
       
    }


    string GetLongUrl(const std::string& shortUrl) {
        io_service io;
        tcp::resolver resolver(io);
        tcp::socket socket(io);

        
            // Подключаемся к серверу базы данных
            connect(socket, resolver.resolve({ "localhost", "8082" }));

            // Формируем запрос для получения длинного URL
            std::ostringstream request_stream;
            request_stream << "GET " << shortUrl << " HTTP/1.1\r\n";
            request_stream << "Host: localhost:8082\r\n";
            request_stream << "Connection: close\r\n\r\n";

            // Отправляем запрос
            write(socket, buffer(request_stream.str()));

            // Получаем и обрабатываем ответ
            boost::asio::streambuf response_buffer;
            boost::asio::read_until(socket, response_buffer, "\r\n");
            istream response_stream(&response_buffer);

            string http_version;
            response_stream >> http_version;
            unsigned int status_code;
            response_stream >> status_code;

            string status_message;
            getline(response_stream, status_message);

            if (status_code == 200) {
                // Успешный ответ, читаем дополнительные данные
                read_until(socket, response_buffer, "\r\n\r\n");
                string dummy_line;
                getline(response_stream, dummy_line);
                getline(response_stream, dummy_line);

                string response_data;
                getline(response_stream, response_data);
                return response_data;
            }
            else {
                // Ошибка в ответе
                cerr << "Error: " << status_code << " " << status_message << endl;
                return "";
            }


        
    }




    void StartServer() {
        io_service io; // объект для ввода/вывода

        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8080)); // прослушивание на ipv4 и порт 8080
        cout << "Сервер успешно запущен на порту 8080. Ожидание входящих соединений..." << endl;
        while (true) { // принимает входящие соединения
            tcp::socket socket(io); // создается сокет
            acceptor.accept(socket);
            cout << "Подключение принято. Ожидание запроса..." << endl; // Уведомление о новом подключении
            boost::system::error_code ignored_error;

            // Чтение http запроса
            boost::asio::streambuf request_buffer; // временное хранилище данных
            boost::asio::read_until(socket, request_buffer, "\r\n\r\n"); // чтение данных из сокета до конца 
            istream request_stream(&request_buffer); // преобразование данных в строку
            string request_line; // хранение строки запроса
            getline(request_stream, request_line); // содержит метод запроса (GET, POST), URL



            if (request_line.find("POST /SHORTEN") != string::npos) { // если пришел POST запрос
                // Извлекаем запрошенную длинную ссылку
                string longUrl;
                while (true) {
                    string line;
                    getline(request_stream, line); // извлекаем каждую строку
                    if (line.empty()) {
                        break;
                    }
                    if (line.find("url=") != string::npos) {
                        // Ищем "URL:" в строке и получаем позицию первого вхождения
                        size_t position = line.find("url=");

                        // Если "URL:" найдено, извлекаем фактическую ссылку
                        if (position != string::npos) {
                            longUrl = line.substr(position + 4); // +4, чтобы пропустить "URL:"
                        }
                    }
                }

                // Генерируем короткую ссылку
                string shortUrl = ShortenUrl(url_decode(longUrl));

                // Отправляем клиенту короткую ссылку
                string response = "HTTP/1.1 200 OK\r\nContent-Length: " + to_string(shortUrl.length()) + "\r\n\r\n" + shortUrl;
                write(socket, buffer(response), ignored_error);
            }
            else if (request_line.find("GET /") != string::npos) {
                // Ищем начало URL после "GET /"
                size_t url_start = request_line.find("GET /") + 5;
                if (url_start != string::npos) {
                    // Ищем пробел после URL
                    size_t url_end = request_line.find(' ', url_start);

                    if (url_end != string::npos) {
                        // Извлекаем короткую ссылку (путь)
                        string url = request_line.substr(url_start, url_end - url_start);


                        // Извлекаем короткую ссылку из пути (между первым и вторым пробелом)
                        string shortUrl = "http://localhost:8080/" + url;

                        // Получаем длинную ссылку
                        string longUrl = GetLongUrl(shortUrl);

                        if (longUrl != "") {
                            // Выполняем перенаправление
                               // Получаем IP-адрес клиента
                            string client_ip = socket.remote_endpoint().address().to_string();
                            //получаем текущее время 
                            auto current_time = std::chrono::system_clock::now();
                            time_t current_time_t = std::chrono::system_clock::to_time_t(current_time);
                            // Преобразуем время в строку
                            string time = ctime(&current_time_t);

                            SendToStatisticServer(client_ip, longUrl, time, shortUrl);
                            string redirect_response = "HTTP/1.1 302 Found\r\nLocation: " + longUrl + "\r\n\r\n";
                            write(socket, buffer(redirect_response), ignored_error);
                        }
                        else {
                            string not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
                            write(socket, buffer(not_found_response), ignored_error);
                        }

                    }
                }
            }
        }
    }

    int main() {
        setlocale(LC_ALL, "RU");
        StartServer();
        return 0;
    }
