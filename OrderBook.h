#pragma once

#include <map>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

#include "Usings.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderbookLevelInfos.h"
#include "Trade.h"
#include "TransactionLog.h"

// Core class of the order book system. It maintains the current state of the system
// Handles adding and cancelling orders, and performs order matching to execute terade when possible
class Orderbook
{
    /*
    2 data structures will be used which is map & unordered_map
    map is chosen because of it is sorted order which can use for bids and asks price
    and for easy access (O(1)) based on the orderId
    */

private:

    struct OrderEntry
    {
        OrderPointer order_{ nullptr };
        OrderPointers::iterator location_; // An iterator to the list of OrderPointer
    };

    // Store the data of a level (Price level)
    struct LevelData
    {
        Quantity quantity_{ };
        Quantity count_{ };

        enum class Action
        {
            Add,
            Remove,
            Match,
        };
    };

    std::unordered_map<Price, LevelData> data_;
    std::map<Price, OrderPointers, std::greater<Price>> bids_; // Descending Order. Key : Price, Value: OrderPointers (List of orderpointer of type "Order")
    std::map<Price, OrderPointers, std::less<Price>> asks_; // Ascending Order
    std::unordered_map<OrderId, OrderEntry> orders_; //Key: OrderId, Value: Content of the order

    // Use for GoodForDay
    mutable std::mutex ordersMutex_;
    std::thread ordersPruneThread_;
    std::condition_variable shutdownConditionVariable_;
    std::atomic<bool> shutdown_{ false };

    void PruneGoodForDayOrders();

    void CancelOrders(OrderIds);
    void CancelOrderInternal(OrderId);

    // Methods relevant for FillOrKill order
    void OnOrderCancelled(OrderPointer);
    void OnOrderAdded(OrderPointer);
    void OnOrderMatched(Price, Quantity, bool);
    void UpdateLevelData(Price, Quantity, LevelData::Action);

    bool CanFullyFill(Side, Price, Quantity) const;
    bool CanMatch(Side, Price) const;
    Trades MatchOrders();

    TransactionLog TransactionLog_;
public:

    Orderbook();
    ~Orderbook();

    // Preventing copis and moves to ensure that only one instance of the Orderbookclass exists, making it a singleton
    Orderbook(const Orderbook&) = delete;
    void operator=(const Orderbook&) = delete;
    Orderbook(Orderbook&&) = delete;
    void operator=(Orderbook&&) = delete;

    Trades AddOrder(OrderPointer);
    void CancelOrder(OrderId);
    Trades ModifyOrder(OrderModify);

    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos() const;

    void prepopulateOrderBook();

    OrderType getRandomOrderType();
    Price getRandomPrice(int, int);
    Quantity getRandomQuantity(int, int);
    void printVisual() const;
    std::string getTransactionLog() const;

    static OrderId id_cnt;
};
