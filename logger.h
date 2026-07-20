#pragma once

#include <string>
#include <fstream>
#include <chrono>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>

namespace LoggerLib{

/* Уровни важности сообщений журнала
 * Уровни упорядочены по убыванию важности:
 * ERROR(0) - наивысший приоритет 
 * INFO(2) - низший приоритет
 * Сообщения с уровнем выше установленного порога не записываются 
 */
	enum class LogLevel{
		ERROR = 0,   // Критические ошибки
		WARNING = 1, // Предупреждения
		INFO = 2	 // Информационные сообщения
	};

/* Структура представляющая одно сообщение журнала */	

	struct LogMessage{
		std::string text;                                // Текст сообщения
		LogLevel level;                                  // Уровень важности
		std::chrono::system_clock::time_point timestamp; // Время получения
	};

/* Функция преобразует уровень важности в строковое представление
 * level - уровень важности */

	std::string levelToString(LogLevel level);

/* Потокобезопасная очередь c блокирующим ожиданием,
 * позволяет безопасно передавать данные между потоками,
 * один поток ожидает появление элемента (waitAndPop),
 * другой добавляет элементы (push) 
 */
	template<typename T>
	class ThreadSafeQueue{
	private:
		mutable std::mutex m_mutex;       // мьютекс для синхранизации доступа, mutable потому что empty() const
		std::queue<T> m_queue;            // базовая очередь
		std::condition_variable m_condvar;// для ожидания/пробуждения
	public:

/* Добавляет элемент в очередь. 
 * Потокобезопасный метод, после добавления пробуждает один поток, 
 * ожидающий на waitAndPop() 
 * value - элемент для добавления (перемещается)
 */
		void push(T value){
			std::lock_guard<std::mutex> lock(m_mutex);
			m_queue.push(std::move(value));
			m_condvar.notify_one(); //Будим один ждущий поток
		}

/* Извлекает элемент, ожидает при пустой очереди,
 * блокирует вызывающий поток до появления элемента,
 * функция возвращает извлеченный элемент
 */
		T waitAndPop(){
			std::unique_lock<std::mutex> lock(m_mutex);
			m_condvar.wait(lock, [this]{return !m_queue.empty();});
			T value = std::move(m_queue.front());
			m_queue.pop();
			return value;
		}

/* Пытается извлечь элемент без ожидания, 
 * value - ссылка для сохранения извлеченного элемента 
 * функция возвращает true - элемент извлечен или false - очередь пуста
 */
		bool tryPop(T& value){
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_queue.empty()){
				return false;
			}
			value = std::move(m_queue.front());
			m_queue.pop();
			return true;
		}

/* Проверяет пуста ли очередь
 * функция возвращает true - если очередь пуста 
 */
		bool empty() const{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_queue.empty();
		}
	};

/* Многопоточный журнал сообщений.
 * Записывает сообщения в файл в фоновом потоке,
 * основной поток только помещает сообщение в очередь и не блокируется 
 */
	class Logger{
		private:
			std::string m_filename;
			std::atomic<LogLevel> m_defaultLevel;//atomic для потокобезопасного чтения/записи
			std::ofstream m_file;
			std::mutex m_fileMutex;//защищает только запись в файл
			
			ThreadSafeQueue<LogMessage> m_queue;
			std::thread m_writerThread;
			std::atomic<bool> m_running{true};

/* Формирует сообщение для записи в файл.
 * формат сообщения: "YYYY-MM-DD HH:MM:SS [LEVEL] текст"
 */
			std::string formatMessage(const LogMessage& msg) const;

/* Функция фонового потока - извлекает сообщения и пишет в файл
 */
			void writerThreadFunc();
		public:

/* Создает логгер и запускает фоновый поток записи
 * filename - имя файла журнала
 * defaultLevel - минимальный уровень записываемых сообщений
 */
			Logger(const std::string& filename, LogLevel defaultLevel);

/* Завершает фоновый поток и закрывает файл */

			~Logger();

/* Запрет копирования */

			Logger(const Logger&) = delete;
			Logger& operator=(const Logger&) = delete;

/* Изменяет уровень важности по умолчанию,
 * level - новый минимальный уровень 
 */
			void setDefaultLevel(LogLevel level);

/* Записывает сообщение с указанным уровнем,
 * если уровень сообщения выше установленного порога, то
 * сообщение игнорируется 
 * message - текст сообщения 
 * level - уровень важности 
 */
			void log(const std::string& message, LogLevel level);

/* Записывает сообщение с уровнем по умолчанию 
 * message - текст сообщения 
 */ 
			void log(const std::string& message);	
	};
}
