#pragma once
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

class TransactionLog
{
public:
	struct Transaction
	{
		std::chrono::system_clock::time_point timestamp;
		std::string description;
	};

	void addTransaction(const std::string& description)
	{
		Transactions_.emplace_back(Transaction{ std::chrono::system_clock::now(), description });
	}	
	
	void clear()
	{
		Transactions_.clear();
	}

	std::string getFormattedLog() const
	{
		std::stringstream ss;
		ss << "Transaction Log:\n";

		for (const auto& transaction : Transactions_)
		{
			auto time = std::chrono::system_clock::to_time_t(transaction.timestamp);
			std::tm tm_;
			localtime_s(&tm_, &time);
			ss << std::put_time(&tm_, "%d/%m/%Y %H:%M:%S") << " - " << transaction.description << std::endl;
		}

		return ss.str();
	}

private:
	std::vector<Transaction> Transactions_;
};

