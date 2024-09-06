/*
Implementation of an Order Book System
Manage and match any buy and sell orders

Order Book: A data structure is collection of orders partition by buy and sell
such that buy and sell order is buy order and sell order are match by price time priority
An order that is the best bid is going to be match first against the sell order thats looking to aggress against the buy side
Key Component of an Order Book:
i. Bid side: List of buy orders with the highest bid price at the top
ii. Ask side:  List of sell orders with the lowest ask price at the top
iii. Orders: List of order whether it is bid or ask
iv. Price Level: We call it as levelinfo at here contains the price and the amount of quantity

Features:
i. Placing an order
ii. Matching the order: See whether does it tally (at a specific price) with the 1 that is saved in the system
iii. Cancel Order
*/
#include "OrderBook.h"

int main() {

    Orderbook orderbook;

    return 0;
}