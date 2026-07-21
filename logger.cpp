#include "logger.h"
#include <sstream>
#include <iomanip>
#include <iostream>

namespace LoggerLib{

// Свободная функция 
	
	std::string levelToString(LogLevel level){
		switch(level){
			case LogLevel::ERROR:   return "ERROR";
			case LogLevel::WARNING: return "WARNING";
			case LogLevel::INFO:    return "INFO";
			default:                return "UNKNOWN";
		}
	}

	std::string Logger::formatMessage(const LogMessage& msg) const{
		std::tm tm_buf;
		auto time = std::chrono::system_clock::to_time_t(msg.timestamp);
		std::stringstream ss;
/*!!! использована потокобезопасная С - функция localtime_r(), всвязи с этим,
 * при компиляции флаг -pthread обязателен, по той причине что он подключает
 * потокобезопасные С - функции */
		ss << std::put_time(localtime_r(&time, &tm_buf), "%Y-%m-%d %H:%M:%S")
		<< " [" << levelToString(msg.level) << "] " << msg.text;
		return ss.str();
	}
//Конструктор со списком инициализации членов класса
	Logger::Logger(const std::string& filename, LogLevel defaultLevel)
		: m_filename(filename)
		, m_defaultLevel(defaultLevel)
	{
// Файл открывается в режиме добавления (не перезаписывает старые логи)
		m_file.open(filename, std::ios::app);
		if(!m_file.is_open()){
			throw std::runtime_error("Failed to open log file " + filename);
		}
/* Запускаем фоновый поток, передаем this - поток будет
 * вызывать writerThreadFunc() как метод этого объекта 
 */
		m_writerThread = std::thread(&Logger::writerThreadFunc, this);
	}
	Logger::~Logger(){
// Сигнал потоку завершится
			m_running = false;

/*Отправляем пустое сообщение, что бы разбудить поток,
 * если он ждет на waitAndPop() 
 */
			m_queue.push(LogMessage{"", LogLevel::INFO, std::chrono::system_clock::now()});

// Дожидаемся завершения потока

			if(m_writerThread.joinable()){
				m_writerThread.join();
			}

// Закрываем файл

			if(m_file.is_open()){
				m_file.close();
			}
		}

		void Logger::setDefaultLevel(LogLevel level){
//atomic - можно присваивать без мьютекса
			m_defaultLevel = level;
		}

		void Logger::log(const std::string& message, LogLevel level){
/* Фильтрация по уровню, записываем только если уровень сообщения
 * не выше (численно не больше) порогового 
 */
			if(level > m_defaultLevel.load()){
				return;
			}
			LogMessage msg;
			msg.text = message;
			msg.level = level;
			msg.timestamp = std::chrono::system_clock::now();
// Перемещаем в очередь. Основная работа функции
			m_queue.push(std::move(msg));
		}
		
		void Logger::log(const std::string& message){
// Делегируем вызов с уровнем по умолчанию
			log(message, m_defaultLevel.load());
		}
		
		void Logger::writerThreadFunc(){
/* Бесконечный цикл обработки сообщений 
 * завершается, когда m_running == false и приходит пустое сообщение 
 */
			while (m_running){
// Блокируется в ожидании новых сообщений
				LogMessage msg = m_queue.waitAndPop();
// Проверка сигнала завершения
				if (!m_running && msg.text.empty()){
					break;
				}
// Критическая сексия, только один поток пишет в файл
				std::lock_guard<std::mutex> lock(m_fileMutex);
				if(!m_file.is_open()){
// На случай если файл закроется по какой то причине
					std::cerr << "Error: log file is not open" << std::endl;
					continue;
				}
				
				std::string formatted = formatMessage(msg);
				m_file << formatted << std::endl;

				if(m_file.fail()){
					std::cerr << "Error: failed to write to log file" << std::endl;
				}
			}
		}
}
