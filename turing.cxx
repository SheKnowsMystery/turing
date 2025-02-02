
#include <cstddef>
#include <cstdint>
#include <string>
#include <set>
#include <map>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <ranges>
#include <cmath>
#include <stdexcept>

using std::operator""s;

// вспомогательная функция для получения строки из потока
auto getline(std::istream& _is) -> std::string {
	std::string str;
	std::getline(_is, str);
	return str;
}

// пространство имён с исключениями
namespace exc {
	class parsing_error : public std::runtime_error {
		using Base = std::runtime_error;
	public:
		using Base::Base;
	};
	class calculating_error : public std::logic_error {
		using Base = std::logic_error;
	public:
		using Base::Base;
	};
	class file_error : public std::runtime_error {
		using Base = std::runtime_error;
	public:
		using Base::Base;
	};
}

class TuringMachine {
	// директивы объявления синонимов типов для упрощённой работы с ними
	using alphabet_t = std::set<char>;
	using symbol_t   = alphabet_t::value_type;
	using command_t  = alphabet_t::value_type;
	using state_t    = std::size_t;
	// правило, разбитое на компоненты
	struct Rule {
		symbol_t  condition;
		symbol_t  replacement;
		command_t command;
		state_t   state;
	};
	using rule_t     = Rule;
	using rules_t    = std::multimap<state_t, rule_t>;
	using tape_t     = std::string;
	using head_t     = std::int64_t;
	// информация об МТ за цикл
	struct CycleData {
		struct {
			symbol_t value;
			head_t   head;
			state_t  state;
		} current, next;
		command_t command;
	};
	using data_t     = CycleData;
	// алфавит, словарь правил, лента
	alphabet_t alphabet;
	rules_t    rules;
	tape_t     tape;
	// состояние и индекс головки
	state_t state = 0;
	head_t  head  = 0;
	// функции для получения конкретных данных из правила в виде строки
	struct get {
		static auto condition(const std::string& _raw) -> symbol_t
		{ return _raw[0]; }
		static auto replacement(const std::string& _raw) -> symbol_t
		{ return _raw[1]; }
		static auto command(const std::string& _raw) -> command_t
		{ return _raw[2]; }
		static auto state(const std::string& _raw) -> state_t {
			try
			{ return std::stoull(_raw.substr(3)); }
			catch (std::logic_error error)
			{ throw exc::parsing_error("Failed to convert string to integer while parsing state for rule \""s + _raw + "\""); }
		}
	};
	// функции для парсинга потоков
	struct parse {
		// алфавит МТ
		static auto alphabet(std::istream& _is) -> alphabet_t {
			alphabet_t dict;
			for (symbol_t symbol : getline(_is))
				dict.insert(symbol);
			return dict;
		}
		// правило МТ
		static auto rule(const std::string& _raw) -> rule_t {
			return {
				.condition   = get::condition(_raw),
				.replacement = get::replacement(_raw),
				.command     = get::command(_raw),
				.state       = get::state(_raw),
			};
		}
		// правила МТ
		static auto rules(std::istream& _is) -> rules_t {
			rules_t dict;
			state_t current;
			bool    begin = false;
			while (!_is.eof()) {
				std::string line = getline(_is);
				if (begin) {
					try
					{ current = std::stoull(line.substr(1)); }
					catch (const std::logic_error& error)
					{ throw exc::parsing_error("Failed to convert string to integer while parsing state \""s + line + "\""); }
					begin = false;
					continue;
				}
				if (line.front() == '~') {
					begin = true;
					continue;
				}
				dict.insert({current, rule(line)});
			}
			return dict;
		}
		// лента МТ
		static auto tape(std::istream& _is) -> tape_t
		{ return getline(_is); }
	};
	// функция проверки ленты на соответствие алфавиту
	void verify() {
		// итерируемся по всем символам в ленте
		for (symbol_t symbol : tape)
			// проверяем на наличие символа в алфавите
			if (!alphabet.contains(symbol))
				// если не нашли, выбрасываем исключение
				throw exc::calculating_error("Encountered unknown symbol \""s + symbol + "\" while verifying tape");
	}
	// функция настройки ленты
	void adjust() {
		std::size_t size = tape.size();
		// получаем абсолютное значение смещения головки относительно 0
		std::size_t offset = std::abs(head);
		// если индекс головки меньше нуля
		if (head < 0) {
			// добавляем в начало ленты столько символов, на сколько индекс головки меньше нуля
			tape.insert(0, tape_t(offset, ' '));
			// обновляем индекс головки на валидный (нулевой)
			head = 0;
			// выходим из функции, чтобы не проверять второе условие
			return;
		}
		// если индекс головки больше текущей длины ленты
		if (head >= size)
			// добавляем в конец ленты столько символов, на сколько индекс головки больше длины ленты
			tape.insert(size, tape_t(offset - (size - 1), ' '));
	}
	// функция нахождения правила
	auto which(symbol_t _symbol) const -> rule_t {
		// получаем все правила для текущего состояния
		auto [begin, end] = rules.equal_range(state);
		// итерируемся через правила, чтобы найти правило для текущего условия
		for (const auto& [state, rule] : std::ranges::subrange(begin, end))
			if (rule.condition == _symbol)
				return rule;
		// если не нашли, выбрасываем исключение
		throw exc::calculating_error("Failed to find fitting rule for symbol \""s + _symbol + "\" at state \"q" + std::to_string(state) + "\"");
	}
	// функция получения следующего индекса головки
	auto next(const command_t& _command) const -> head_t {
		switch (_command) {
		case '<': return head - 1;
		case '>': return head + 1;
		case '.': return head;
		default: throw exc::calculating_error("Unrecognized moving operator \""s + _command + "\" in command");
		}
	}
	// функция стилизации команды чтобы соответствовать требованиям задания
	static auto stylize(const command_t& _command)  -> char {
		switch (_command) {
		case '<': return 'L';
		case '>': return 'R';
		case '.': return 'E';
		default: throw exc::calculating_error("Unrecognized moving operator \""s + _command + "\" in command");
		}
	}
	// функция вычисления информации об МТ за цикл
	auto calculate(const rule_t& _rule) const -> data_t {
		return {
			// сохраняем информацию об МТ в данный момент
			.current = {
				.value = tape[head],
				.head  = head,
				.state = state,
			},
			// вычисляем информацию об изменениях МТ
			.next = {
				.value = _rule.replacement,
				.head  = next(_rule.command),
				.state = _rule.state,
			},
			// сохраняем команду для последующего логирования
			.command = _rule.command,
		};
	}
	// функция применения изменений МТ
	void apply(data_t _data) {
		tape[head] = _data.next.value;
		head       = _data.next.head;
		state      = _data.next.state;
	}
	// функция логирования
	void info(std::ostream& _os) const {
		_os << '>' << std::setw(tape.size()) << std::setfill('-') << '|' << std::endl;
		_os << tape << std::endl;
		_os << std::setw(head + 1) << std::setfill(' ') << '^' << std::endl;
	}
	// функция логирования с командой
	void info(std::ostream& _os, data_t _data) const {
		// информация об МТ
		info(_os);
		// выполненная команда
		_os
			<< "q" << _data.current.state
			<< '~' << _data.current.value
			<< "->"
			<< "q" << _data.next.state
			<< "~" << _data.next.value
			<< stylize(_data.command) << std::endl;
	}
public:
	TuringMachine(
		std::istream& _alphabet,
		std::istream& _rules,
		std::istream& _tape
	) :
	// инициализация МТ путём парсинга потоков
		alphabet(parse::alphabet(_alphabet)),
		rules(parse::rules(_rules)),
		tape(parse::tape(_tape))
	// проверяем вводные данные
	{ verify(); }
	// функция выполнения МТ
	void process(std::ostream& _os) {
		// устанавливаем значения для состояния и головки по умолчанию
		state = 1;
		head  = 0;
		// цикл вычислений работает, пока состояние не установлено на 0
		// что будет свидетельствовать об остановке
		while (state != 0) {
			// настраиваем длину ленты, чтобы индекс головки не вышел за её пределы
			adjust();
			// получаем правило, по которому будет осуществляться вычисление на этом цикле
			rule_t rule = which(tape[head]);
			// вычисляем информацию об МТ на этом цикле
			data_t data = calculate(rule);
			// записываем информацию об этом цикле в предоставленный поток
			info(_os, data);
			// применяем изменения, которые вычислены с помощью полученного правила
			apply(data);
		}
		// записываем финальное состояние ленты в предоставленный поток
		info(_os);
	}
};

int main() {
	// во время работы МТ и работы с файлами могут выброситься исключения
	// блок чтобы поймать и обработать эти исключения
	try {
		// файлы на вход
		auto alphabet = std::ifstream("alphabet.txt");
		if (alphabet.fail())
			throw exc::file_error(R"(Failed to open file "alphabet.txt")");
		auto rules = std::ifstream("rules.txt");
		if (rules.fail())
			throw exc::file_error(R"(Failed to open file "rules.txt")");
		auto tape = std::ifstream("tape.txt");
		if (tape.fail())
			throw exc::file_error(R"(Failed to open file "tape.txt")");
#ifndef _DEBUG
		// файл на выход
		auto out = std::ofstream("out.txt", std::ios::trunc);
		if (out.fail())
			throw exc::file_error(R"(Failed to open file "out.txt")");
#endif
		// инициализируем МТ
		auto mt = TuringMachine(alphabet, rules, tape);

		// запускаем вычисления и передаём поток для логирования в выходной файл
#ifdef _DEBUG
		mt.process(std::cout);
#else
		mt.process(out);
#endif
	}
	catch (const exc::file_error& error)
	{ std::cerr << "FILE ERROR: " << error.what() << std::endl; }
	catch (const exc::parsing_error& error)
	{ std::cerr << "PARSING ERROR: " << error.what() << std::endl; }
	catch (const exc::calculating_error& error)
	{ std::cerr << "CALCULATING ERROR: " << error.what() << std::endl; }

	return 0;
}