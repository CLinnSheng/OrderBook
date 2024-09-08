#include "OrderBook.h"
#include <numeric>
#include <chrono>
#include <ctime>
#include <random>
#include <iostream>
#include <locale>
#include <iomanip>

void Orderbook::PruneGoodForDayOrders()
{
	using namespace std::chrono;
	const auto end = hours(16);

	// Thread is spinning at a loop until 4pm to cancel all GoodForDays order
	while (true)
	{
		const auto now = system_clock::now(); // Getting the current time
		const auto now_c = system_clock::to_time_t(now); // Convert the object time to time_t
		std::tm now_parts;
		
		localtime_s(&now_parts, &now_c); // Convert time_t object to tm to enable time to be break down into years, month, etc..

		// only executed if it past 4pm
		if (now_parts.tm_hour >= end.count())
			now_parts.tm_mday += 1; // if it past 4pm add 1 more day to it because the market is closed, prunning the order


		// We setting the market closed at 4pm
		now_parts.tm_hour = end.count();
		now_parts.tm_min = 0;
		now_parts.tm_sec = 0;

		// Caculate how much time until 4pm
		auto next = system_clock::from_time_t(mktime(&now_parts));
		auto till = next - now + milliseconds(100); // Add 100ms just to make sure we do right after our pruning point in time

		{
			/*
			Dont want this thread to alter the state of our data structure at the same time
			Any time we reference our orders, we need to take tat reference with a lock (protect the data)
			*/
			std::unique_lock ordersLock{ ordersMutex_ };

			// If orderbook is shutdown or shutdown before the market close
			// we straigth return because we cant do anything
			if (shutdown_.load(std::memory_order_acquire) ||
				shutdownConditionVariable_.wait_for(ordersLock, till) == std::cv_status::no_timeout)
				return;
		}

		// Proceed after reach 4pm
		OrderIds orderIds;

		{
			std::scoped_lock ordersLock{ ordersMutex_ };

			// Iterate every our orders we have outstanding
			// Collect the order Id of GoodForDay order because this method is particularly for this type of order only
			for (const auto& [_, entry] : orders_)
			{
				const auto& [order, __] = entry;

				if (order->GetOrderType() != OrderType::GoodForDay)
					continue;

				orderIds.push_back(order->GetOrderId());
				TransactionLog_.addTransaction("GoodForDay order " + std::to_string(order->GetOrderId()) + " removed due to expiration");
			}
		}

		CancelOrders(orderIds);
	}
}

void Orderbook::CancelOrders(OrderIds orderIds)
{
	/*
	Reason to have this extra function just to improve efficiency & prevent overhead
	as we only need to lock one time regardless the number of order
	*/
	std::scoped_lock ordersLock{ ordersMutex_ };

	for (const auto& orderId : orderIds)
		CancelOrderInternal(orderId);
}

void Orderbook::CancelOrderInternal(OrderId orderId)
{
	if (!orders_.count(orderId))
		return;

	const auto [order, iterator] = orders_.at(orderId);
	orders_.erase(orderId);

	if (order->GetSide() == Side::Sell)
	{
		auto price = order->GetPrice();
		auto& orders = asks_.at(price);
		orders.erase(iterator); // Erasing the particular order for the price and the iterator point to
		if (orders.empty()) // If there is no more order in this price point
			asks_.erase(price); // straight delete the the key and value pair in the hash
	}
	else
	{
		auto price = order->GetPrice();
		auto& orders = bids_.at(price);
		orders.erase(iterator);
		if (orders.empty())
			bids_.erase(price);
	}

	TransactionLog_.addTransaction("Order " + std::to_string(orderId) + " cancelled");
	OnOrderCancelled(order);
}

void Orderbook::OnOrderCancelled(OrderPointer order)
{
	UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
}

void Orderbook::OnOrderAdded(OrderPointer order)
{
	UpdateLevelData(order->GetPrice(), order->GetInitialQuantity(), LevelData::Action::Add);
}

void Orderbook::OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled)
{
	// If the order is fully filled, we need to remove that order count from our bookkeeping data structure
	// if not fully filled just dont touch the bookkeeping
	UpdateLevelData(price, quantity, isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match);
}

void Orderbook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action)
{
	auto& data = data_[price];

	// Modify the information of level data accodring to the action
	// If the action is match we just need to remove the quantity, we dont need to remove the count because the order is still not fully filled
	data.count_ += action == LevelData::Action::Remove ? -1 : action == LevelData::Action::Add ? 1 : 0;
	if (action == LevelData::Action::Remove || action == LevelData::Action::Match)
	{
		data.quantity_ -= quantity;
	}
	else
	{
		data.quantity_ += quantity;
	}

	// if there is no more orders remaining in this level, just remove the whole level
	if (data.count_ == 0)
		data_.erase(price);
}

bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
/*
Basically work the same as CanMatch but this 1 is specifically designed for CanFullyFill or not the match order
*/
	if (!CanMatch(side, price))
		return false;

	std::optional<Price> threshold;

	if (side == Side::Buy)
	{
		// Getting the range of asks price for the person [buy price, best ask (cheapest)]
		const auto [askPrice, _] = *asks_.begin();
		threshold = askPrice;
	}
	else
	{
		const auto [bidPrice, _] = *bids_.begin();
		threshold = bidPrice;
	}

	for (const auto& [levelPrice, levelData] : data_)
	{
		if (threshold.has_value() &&
			(side == Side::Buy && threshold.value() > levelPrice) ||
			(side == Side::Sell && threshold.value() < levelPrice))
			continue;

		if ((side == Side::Buy && levelPrice > price) ||
			(side == Side::Sell && levelPrice < price))
			continue;

		// if this level of quantity is enough to fill the order straight return
		if (quantity <= levelData.quantity_)
			return true;

		// Keep repeating fill the order if this level of quantity isnt enuf to fully filled
		quantity -= levelData.quantity_;
	}

	// Coulnt fully filled
	return false;
}

bool Orderbook::CanMatch(Side side, Price price) const
{
/*
This function is to check whether we can match this order in the orderbook or not (eg: Match a buy order to a sell order)
*/
	if (side == Side::Buy)
	{
		// if no 1 is selling then fail
		if (asks_.empty())
			return false;

		// the lowest price that people offer to sell
		const auto& [bestAsk, _] = *asks_.begin();
		return price >= bestAsk;
	}

	else
	{
		// Selling
		// if no 1 is requesting to buy then fail
		if (bids_.empty())
			return false;

		// the highest price people offer to buy
		const auto& [bestBid, _] = *bids_.begin();
		return price <= bestBid;
	}
}

Trades Orderbook::MatchOrders()
{
	// See whether the bestBid and bestAsk can match or not
	Trades trades;
	trades.reserve(orders_.size());

	while (true)
	{
		if (bids_.empty() || asks_.empty())
			break;

		auto& [bidPrice, bids] = *bids_.begin();  // Getting the highest price people offer to buy
		auto& [askPrice, asks] = *asks_.begin();  // Getting the lowest price people offer to sell

		// asks & bids are a list of Orders (shared_ptr)

		// it doesnt make sense if u want to buy the price higher than the 1 u desired
		if (bidPrice < askPrice)
			break;

		while (!bids.empty() && !asks.empty())
		{
			auto bid = bids.front();   // The first in queue for the highest price people offer to buy
			auto ask = asks.front();   // The first in queue for the lowest price people offer to sell

			Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

			bid->Fill(quantity);
			ask->Fill(quantity);

			// if the order in bid is been filled
			// we just remove it from the list of orders as well as in bids
			if (bid->IsFilled())
			{
				bids.pop_front();
				orders_.erase(bid->GetOrderId());
			}

			// Same goes to ask
			if (ask->IsFilled())
			{
				asks.pop_front();
				orders_.erase(ask->GetOrderId());
			}


			trades.push_back(Trade{
				TradeInfo{ bid->GetOrderId(), bid->GetPrice(), quantity },
				TradeInfo{ ask->GetOrderId(), ask->GetPrice(), quantity }
				});

			OnOrderMatched(bid->GetPrice(), quantity, bid->IsFilled());
			OnOrderMatched(ask->GetPrice(), quantity, ask->IsFilled());
		}

		if (bids.empty())
		{
			bids_.erase(bidPrice);
			data_.erase(bidPrice);
		}

		if (asks.empty())
		{
			asks_.erase(askPrice);
			data_.erase(askPrice);
		}
	}

	if (!bids_.empty())
	{
		auto& [_, bids] = *bids_.begin();
		auto& order = bids.front();
		if (order->GetOrderType() == OrderType::FillAndKill)
			CancelOrder(order->GetOrderId());
	}

	if (!asks_.empty())
	{
		auto& [_, asks] = *asks_.begin();
		auto& order = asks.front();
		if (order->GetOrderType() == OrderType::FillAndKill)
			CancelOrder(order->GetOrderId());
	}

	for (const auto& trade : trades) 
	{
		TransactionLog_.addTransaction("Trade executed: Bid " + std::to_string(trade.GetBidTrade().orderdId_) +
			" matched with Ask " + std::to_string(trade.GetAskTrade().orderdId_) +
			" for " + std::to_string(trade.GetBidTrade().quantity_) +
			" @ $" + std::to_string(trade.GetBidTrade().price_));
	}

	return trades;
}

Orderbook::Orderbook() : ordersPruneThread_{ [this] { PruneGoodForDayOrders(); } }
{
	// When an orderbook is created, a new thread is also created.
	// The purpose of this thread is to wait till the end of day, for every order that is GoodForDay
	// All the order will be cancel
	prepopulateOrderBook();
}

Orderbook::~Orderbook()
{
	shutdown_.store(true, std::memory_order_release);
	shutdownConditionVariable_.notify_one();
	ordersPruneThread_.join();
}

Trades Orderbook::AddOrder(OrderPointer order)
{
	/*
	This function add order to the orderbook
	First of all we have to check what kind of order type is:
	a) Market
	- Looking to Buy, only valid theres someone selling. If there arent any sell, we just return empty Trade (nothing happen) --> Order is cancelled
	- If there exists some1 asking to sell, theoretically the worst asks will be executed on
	- Then pass on to good till cancel order
	*/
	std::scoped_lock ordersLock{ ordersMutex_ };

	// if contain this orderId already, we have to reject it because each order has an unique orderId
	if (orders_.count(order->GetOrderId()))
		return { };

	// Deals with OrderType::Market
	if (order->GetOrderType() == OrderType::Market)
	{
		// Buying at market rate will lead to buying the highest ask price
		if (order->GetSide() == Side::Buy && !asks_.empty())
		{
			// Then proceed to handling GoodTillCancel Order type because if there is more qty in the book than the 1 u requested
			// your Market order will be filled fully and then remove from the oder book or else
			// it will fill with what left in the book and then become a limit order
			const auto& [worstAsk, _] = *asks_.rbegin();
			order->ToGoodTillCancel(worstAsk);
			// Change the price to the best price which is the lowest people offer because market order
			// we buying at the cheapest option (Want to buy no matter what according to market rate)
		}

		// Same goes to sell will lead to the lowest bid price
		else if (order->GetSide() == Side::Sell && !bids_.empty())
		{
			const auto& [worstBid, _] = *bids_.rbegin();
			order->ToGoodTillCancel(worstBid);
		}
		else
			return { };
	}

	if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
		return { };

	if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetInitialQuantity()))
	{
		TransactionLog_.addTransaction("FillOrKill order " + std::to_string(order->GetOrderId()) + " rejected - cannot be fully filled");
		return { };
	}

	OrderPointers::iterator iterator; // Pointed to the last order

	if (order->GetSide() == Side::Buy)
	{
		auto& orders = bids_[order->GetPrice()];
		orders.push_back(order);
		iterator = std::prev(orders.end());
	}
	else
	{
		auto& orders = asks_[order->GetPrice()];
		orders.push_back(order);
		iterator = std::prev(orders.end());
	}

	orders_.insert({ order->GetOrderId(), OrderEntry{ order, iterator } });
	TransactionLog_.addTransaction("Order " + std::to_string(order->GetOrderId()) + " added");
	OnOrderAdded(order);

	// When a new order is added to the orderbook, there's a possibility that it can be immediately matched with existing orders 
	//on the opposite side. By calling MatchOrders() right after adding the new order, we ensure that any potential trades are executed without delay.
	return MatchOrders();
}

void Orderbook::CancelOrder(OrderId orderId)
{
	std::scoped_lock ordersLock{ ordersMutex_ };
	CancelOrderInternal(orderId);
}

Trades Orderbook::ModifyOrder(OrderModify order)
{
	OrderType orderType;

	{
		std::scoped_lock ordersLock{ ordersMutex_ };

		if (!orders_.count(order.GetOrderId()))
			return { };

		const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
		orderType = existingOrder->GetOrderType();
	}

	CancelOrder(order.GetOrderId());
	return AddOrder(order.ToOrderPointer(orderType));
}

std::size_t Orderbook::Size() const
{
	std::scoped_lock ordersLock{ ordersMutex_ };
	return orders_.size();
}

OrderbookLevelInfos Orderbook::GetOrderInfos() const
{
	LevelInfos bidInfos, askInfos;
	bidInfos.reserve(orders_.size());
	askInfos.reserve(orders_.size());

	auto CreateLevelInfos = [](Price price, const OrderPointers& orders)
		{
			return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
				[](Quantity runningSum, const OrderPointer& order)
				{ return runningSum + order->GetRemainingQuantity(); }) };
		};

	for (const auto& [price, orders] : bids_)
		bidInfos.push_back(CreateLevelInfos(price, orders));

	for (const auto& [price, orders] : asks_)
		askInfos.push_back(CreateLevelInfos(price, orders));

	return OrderbookLevelInfos{ bidInfos, askInfos };
}

void Orderbook::prepopulateOrderBook()
{
	// Prepopulate the orderbook with 10 bid & 10 ask
	for (int i = 0; i < 10; i++)
	{
		OrderType OrderType_ = getRandomOrderType();
		Price Price_ = getRandomPrice(90, 100);
		Quantity Quantitiy_ = getRandomQuantity(50, 100);

		OrderPointer order = std::make_shared<Order>(OrderType_, id_cnt++, Side::Buy, Price_, Quantitiy_);
		AddOrder(order);
	}

	for (int i = 0; i < 10; i++)
	{
		OrderType OrderType_ = getRandomOrderType();
		Price Price_ = getRandomPrice(100, 110);
		Quantity Quantitiy_ = getRandomQuantity(50, 100);

		OrderPointer order = std::make_shared<Order>(OrderType_, id_cnt++, Side::Sell, Price_, Quantitiy_);
		AddOrder(order);
	}
}

OrderType Orderbook::getRandomOrderType()
{
// Prepoulate the orderbook with GoodTillCancel & GoodForDay order

	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_int_distribution<> dis(0, 1);

	switch (dis(gen))
	{
		case 0: return OrderType::GoodTillCancel;
		case 1: return OrderType::GoodForDay;
	}
}

Price Orderbook::getRandomPrice(int min, int max)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(min, max);

	return static_cast<Price>(dis(gen));
}

Quantity Orderbook::getRandomQuantity(int min, int max)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(min, max);

	return static_cast<Quantity>(dis(gen));
}

void Orderbook::printVisual() const 
{
	using namespace std::chrono;
	const auto now = system_clock::now(); // Getting the current time
	const auto now_c = system_clock::to_time_t(now); // Convert the object time to time_t
	std::tm now_parts;
	localtime_s(&now_parts, &now_c); // Convert time_t object to tm to enable time to be break down into years, month, etc..

	std::cout << now_parts.tm_mday << '/' << now_parts.tm_mon + 1 << '/' << (now_parts.tm_year + 1900) % 2000 << "\t"
		<< now_parts.tm_hour << ':' << now_parts.tm_min << ':' << now_parts.tm_sec << "\n\n";

	auto infos = GetOrderInfos();

	auto bids = infos.GetBids();
	auto asks = infos.GetAsks();
	size_t maxSize = std::max(bids.size(), asks.size());

	std::locale::global(std::locale("en_US.UTF-8"));
	std::wcout.imbue(std::locale());
	std::cout << std::left;

	std::cout << "============== BIDS ==============\n";
	for (const auto& bid : bids)
	{
		std::string color = "32";
		std::cout << "\033[1;" << color << "m" << "$" << std::setw(6) << bid.price_ << std::setw(5) << bid.quantity_ << "\033[0m ";

		for (int i = 0; i < bid.quantity_ / 10; i++)
			std::wcout << L"█";
		
		std::cout << std::endl;
	}

	std::cout << std::endl;
	std::cout << "============== ASKS ==============\n";
	for (const auto& ask : asks)
	{
		std::string color = "31";
		std::cout << "\033[1;" << color << "m" << "$" << std::setw(6) << ask.price_ << std::setw(5) << ask.quantity_ << "\033[0m ";

		for (int i = 0; i < ask.quantity_ / 10; i++)
			std::wcout << L"█";

		std::cout << std::endl;
	}
}

std::string Orderbook::getTransactionLog() const { return TransactionLog_.getFormattedLog(); }
