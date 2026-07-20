/* Консольное приложение для тестирования библиотеки записи сообщений в журнал 
 * Принимает сообщение от пользователя и записывает их в файл через 
 * многопоточную динамическую библиотеку liblogger.so
 * 
 * Использование ./logger_app <имя файла> [уровень сообщения]
 * 
 * Команды для работы с приложением:
 * <текст> [уровень] - записать сообщение
 * !level <уровень>  - изменить уровень по умолчанию
 * !level            - показать текущий уровень
 * !help             - показать справку
 * !exit или Ctrl+C  - выход*/

#include "logger.h"
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <atomic>
#include <csignal>

//Глобальный флаг для обработки Ctrl+C
std::atomic<bool> g_running{true};

/*Функция обработчик сигнала SIGIN, необходимо для
 * без аварийного завершения работы программы, корректного вызова деструктора
 * и высвобождении системных ресурсов */ 
void signalHandler(int signal){
	if(signal == SIGINT){
		std::cout << "\nПолучен сигнал завершения программы. Выход..." << std::endl;
		g_running = false;
	}
}

//Функция для парсинга уровня из строки, бросает исключение при нераспознаной строке
LoggerLib::LogLevel parseLevel(const std::string& str){
	if(str == "ERROR" || str == "error" || str == "0"){
		return LoggerLib::LogLevel::ERROR;
	}
	if(str == "WARNING" || str == "warning" || str == "1"){
		return LoggerLib::LogLevel::WARNING;
	}
	if(str == "INFO" || str == "info" || str == "2"){
		return LoggerLib::LogLevel::INFO;
	}
	throw std::runtime_error("Неизвестный уровень: " + str);
}
//Справка по программе
void printHelp(){
	std::cout << "Команды:\n" 
			  << "  <текст> [ERROR|WARNING|INFO]  - запись сообщения\n" 
			  << "  !level <уровень>              - изменить уровень по умолчанию\n" 
			  << "  !help                         - показать эту справку\n" 
              << "!exit или Ctrl+C                - выход из программы\n"; 
}

int main(int argc, char* argv[]){
// Параметры командной строки
	if(argc < 2){
		std::cerr << "Использование: " << argv[0]
				  << " <файл журнала> [уровень по умолчанию]i\n" 
				  << " уровень: ERROR, WARNING, INFO (по умолчанию INFO)\n";
		return 1;
	}

	std::string logFile = argv[1];
	LoggerLib::LogLevel defaultLevel = LoggerLib::LogLevel::INFO;

	if(argc >=3){
		try{
			defaultLevel = parseLevel(argv[2]);
		} catch (const std::exception& e) {
			std::cerr << "Ошибка: " << e.what() << std::endl;
			return 1;
		}
	}
//Установка обработчика сигнала
	std::signal(SIGINT, signalHandler);

	try{
//Создаем логгер и запускаем фоновый поток в конструкторе
		LoggerLib::Logger logger(logFile, defaultLevel);
		std::cout << "Логгер запущен\n"
				  << "Файл журнала: " << logFile << "\n"
				  << "Уровень по умолчанию: " << levelToString(defaultLevel) << std::endl;
		printHelp();

		std::string input;
		while(g_running){
			std::cout << "> ";
			if(!std::getline(std::cin, input)){
				break;
			}
			if(input.empty()){
				continue;// Игнорируем пустые строки
			}
//Обработка команд начинается с '!'
			if(input[0] == '!'){
				std::istringstream iss(input.substr(1));
				std::string command;
				iss >> command;

				if(command == "exit"){
					std::cout << "Завершение работы..." << std::endl;
					break;
				} else if(command == "help") {
					printHelp();
				} else if(command == "level") {
					std::string levelStr;
					iss >> levelStr;
					if(levelStr.empty()){
						std::cout << "Текущий уровень: " << levelToString(defaultLevel) << std::endl;
					} else {
						try{
							defaultLevel = parseLevel(levelStr);
							logger.setDefaultLevel(defaultLevel);
							std::cout << "Уровень изменен на: "
								      << levelToString(defaultLevel) << std::endl;
						} catch (const std::exception& e){
							std::cout << "Ошибка: " << e.what() << std::endl;
						}
				}	
			} else {
				std::cout << "Неизвестная команда: " << command << std::endl;
				printHelp();
			}
			continue;
		}
/* Разбор сообщения, ищем уровень в последнем слове 
 * поддерживает форматы "ERROR" "error" "0"
 * если последнее слово не распознано как уровень, то оно считается
 * частью сообщения */
			std::string text;
			LoggerLib::LogLevel level = defaultLevel;

//Поиск последний токен, являющийся уровнем
			size_t lastSpace = input.find_last_of(' ');
			if(lastSpace != std::string::npos){
				std::string lastWord = input.substr(lastSpace + 1);
				try{
					level = parseLevel(lastWord);
					//Если уровень распознан, то отделяем текст
					text = input.substr(0, lastSpace);
				} catch (const std::runtime_error&){
					//Не уровень, значит все сообщение целиком
					text = input;
				}
			} else {
				text = input;
			}
			//Отправка в логгер
			logger.log(text, level);
			std::cout << "Сообщение отправлено в очередь" << std::endl;
		}
	} catch (const std::exception& e) {
		std::cerr << "Критическая ошибка: " << e.what() << std::endl;
		return 1;
	}	
	std::cout << "Приложение завершено." << std::endl;
}
