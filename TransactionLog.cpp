#include "TransactionLog.h"
void TransactionLog::addTransaction(const std::string& description)
{
	Transactions_.emplace_back(Transaction{ std::chrono::system_clock::now(), description });
}

void TransactionLog::clear()
{
	Transactions_.clear();
}

std::string TransactionLog::getFormattedLog() const
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
