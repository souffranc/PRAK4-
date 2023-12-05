#include <iostream>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>

using namespace std;
using namespace boost::asio;
using ip::tcp;
using json = nlohmann::json;



class HashTable {
private:
    static const int TABLE_SIZE = 100;  
    struct Node {
        string key;
        string value;
        Node* next;
        Node(const string& k, const string& v) : key(k), value(v), next(nullptr) {}
    };

    vector<Node*> table;

    //хэш функция
    size_t hash(const string& key) const {
        size_t hash = 0;
        for (char c : key) {
            hash = (hash * 7) + c;
        }
        return hash % TABLE_SIZE;
    }

public:
    //конструктор
    HashTable() : table(TABLE_SIZE, nullptr) {}

    //деструктор
    ~HashTable() {
        for (Node* entry : table) {
            while (entry != nullptr) {
                Node* temp = entry;
                entry = entry->next;
                delete temp;
            }
        }
    }
  
    //для проверки наличия значения в хэш-таблице
    string getKeyForValue(const string& longUrl) const {
        for (const Node* entry : table) {
            const Node* current = entry;
            while (current != nullptr) {
                if (current->value == longUrl) {
                    return current->key;
                }
                current = current->next;
            }
        }
        return "";  // Значение не найдено
    }


    //вставка в хэш таблицу
    void insert(const string& key, const string& value) {
        size_t index = hash(key);
        Node* newNode = new Node(key, value);
        newNode->next = table[index];
        table[index] = newNode;
    }

    //забрать из хэш таблицы
    string get(const string& key) const {
        size_t index = hash(key);
        Node* entry = table[index];
        while (entry != nullptr) {
            if (entry->key == key) {
                return entry->value;
            }
            entry = entry->next;
        }
        return "";  // ключ не найден
    }
};



string GenerateRandomShortUrl() {
    static const string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static random_device rd;
    static mt19937 gen(rd());


    uniform_int_distribution<> length_dis(2, 6);
    int length = length_dis(gen);


    uniform_int_distribution<> dis(0, characters.length() - 1);
    string shortUrl;
    for (int i = 0; i < length; ++i) {
        shortUrl += characters[dis(gen)];
    }
    return shortUrl;
}


HashTable urlDatabase;


    void AddToDatabase(const string& shortUrl, const string& longUrl) {

        urlDatabase.insert(shortUrl, longUrl);
    }


    string ProcessShortenRequest(const string& longUrl, HashTable& urlDatabase) {
        // Проверяем, есть ли длинная ссылка уже в базе данных
        string existingShortUrl = urlDatabase.getKeyForValue(longUrl);

        // Если длинная ссылка уже существует, возвращаем соответствующую короткую
        if (!existingShortUrl.empty()) {
            return existingShortUrl;
        }

        // Генерируем новую короткую ссылку
        string shortUrl = "http://localhost:8080/" + GenerateRandomShortUrl();

        // Добавляем запись в базу данных
        urlDatabase.insert(shortUrl, longUrl);

        // Возвращаем новую короткую ссылку
        return shortUrl;
    }



string GetLongUrl(const string& shortUrl) {
    // Поиск в хэш-таблице
    string longUrl = urlDatabase.get(shortUrl);
    return longUrl;
}


int main() {
    io_service io;
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 8082));

    std::cout << "Database server is running on port 8082. Waiting for incoming connections..." << std::endl;

    while (true) {
        tcp::socket socket(io);
        acceptor.accept(socket);

       boost::asio::streambuf request_buffer;
        boost::asio::read_until(socket, request_buffer, "\r\n");
        istream request_stream(&request_buffer); 
        std::string request_line;
        getline(request_stream, request_line);


        if (request_line.find("POST /shorten") != std::string::npos) {
            std::string longUrl;
            while (true) {
                std::string line;
                getline(request_stream, line);
                if (line.empty()) {
                    break;
                }
                if (line.find("longUrl") != std::string::npos) {
                    size_t position = line.find("longUrl");
                    if (position != std::string::npos) {
                        longUrl = line.substr(position + 8);
                    }
                }
            }

            // Обрабатываем запрос на сокращение URL
            std::string response_data = ProcessShortenRequest(longUrl, urlDatabase);

            // Отправляем ответ на сервер сокращения URL
            std::ostringstream response_stream;
            response_stream << "HTTP/1.1 200 OK\r\n";
            response_stream << "Content-Length: " << response_data.length() << "\r\n";
            response_stream << "\r\n";
            response_stream << response_data;

            write(socket, buffer(response_stream.str()));
        }
        // Добавляем обработку GET-запроса
        else if (request_line.find("GET ") != std::string::npos) {
            std::string shortUrl = request_line.substr(4); // Извлекаем короткую ссылку из GET-запроса
            shortUrl = shortUrl.substr(0, shortUrl.find(" ")); // Очищаем от лишних символов

            // Получаем длинную ссылку
            std::string longUrl = GetLongUrl(shortUrl);


            std::ostringstream response_stream;
            response_stream << "HTTP/1.1 200 OK\r\n";
            response_stream << "Content-Length: " << longUrl.length() << "\r\n";
            response_stream << "\r\n";
            response_stream << longUrl;

            write(socket, buffer(response_stream.str()));
        }
    }

    return 0;
}
    