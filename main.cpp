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
#include "Constants.h"

#include <iostream>
#include <iomanip>

OrderId Orderbook::id_cnt = 0;

void Login_Screen()
{
    std::cout << "===============================\n"
              << "       ORDER BOOK SYSTEM       \n"
              << "===============================\n";
    std::cout << "Welcome to the Order Book System\n";

    std::cout << "\nPlease enter your option:\n";
    std::cout << "1. Print Orderbook\n"
              << "2. Print Transaction Log\n"
              << "3. Add Order\n"
              << "4. Exit\n";
    std::cout << "Choice: ";
}

void clearConsole() 
{
    #if defined(_WIN32) || defined(_WIN64)
        system("cls"); // Windows
    #else
        system("clear"); // Unix/Linux/MacOS
    #endif
}

void Handle_Add(std::shared_ptr<Orderbook> orderbook)
{
    int Input_Side, Input_OrderType, Input_Price, Input_Quantity;

    std::cout << "Current OrderId: " << orderbook->id_cnt << std::endl;
    std::cout << "Enter Side:" << std::endl
              << "1. Buy" << std::endl
              << "2. Sell" << std::endl;
    std::cout << "Selection: ";
    std::cin >> Input_Side;
    if (Input_Side < 0 or Input_Side > 2)
        throw std::logic_error("Unsupport Side");

    std::cout << "\nEnter Order Type:" << std::endl
              << "1. Good Till Cancel" << std::endl
              << "2. Fill And Kill" << std::endl
              << "3. Fill Or Kill" << std::endl
              << "4. GoodForDay" << std::endl
              << "5. Market" << std::endl;
    std::cout << "Selection: ";
    std::cin >> Input_OrderType;
    if (Input_OrderType < 0 or Input_OrderType > 5)
        throw std::logic_error("Unsupport Order Type");

    if (Input_OrderType != 5)
    {
        std::cout << "\nEnter Price: ";
        std::cin >> Input_Price;
        if (Input_Price <= 0)
            throw std::logic_error("Price must be greater than 0");
    }

    std::cout << "\nEnter Quantity: ";
    std::cin >> Input_Quantity;
    if (Input_Quantity <= 0)
        throw std::logic_error("Quantity must be greater than 0");

    orderbook->AddOrder(std::make_shared<Order>(static_cast<OrderType>((int)Input_OrderType - 1), orderbook->id_cnt++, static_cast<Side>((int)Input_Side - 1), (Input_OrderType != 5)? static_cast<Price>(Input_Price) : Constants::InvalidPrice, static_cast<Quantity>(Input_Quantity)));
}

void Handle_Modify(std::shared_ptr<Orderbook> orderbook)
{
    int Input_OrderId, Input_Side, Input_Price, Input_Quantity;

    std::cout << "Enter OrderId: ";
    std::cin >> Input_OrderId;

    std::cout << "Enter Side:" << std::endl
              << "1. Buy" << std::endl
              << "2. Sell" << std::endl;
    std::cout << "Selection: ";
    std::cin >> Input_Side;
    if (Input_Side < 0 or Input_Side > 2)
        throw std::logic_error("Unsupport Side");

    std::cout << "\nEnter Price: ";
    std::cin >> Input_Price;
    if (Input_Price <= 0)
        throw std::logic_error("Price must be greater than 0");

    std::cout << "\nEnter Quantity: ";
    std::cin >> Input_Quantity;
    if (Input_Quantity <= 0)
        throw std::logic_error("Quantity must be greater than 0");
    
    orderbook->ModifyOrder(OrderModify{ static_cast<OrderId>(Input_OrderId), static_cast<Side>((int)Input_Side - 1), static_cast<Price>(Input_Price), static_cast<Quantity>(Input_Quantity) });
}

void Handle_Cancel(std::shared_ptr<Orderbook> orderbook)
{
    int Input_OrderId;

    std::cout << "Enter OrderId: ";
    std::cin >> Input_OrderId;

    orderbook->CancelOrder(static_cast<OrderId>(Input_OrderId));
}

void Print_TransactionLog(std::shared_ptr<Orderbook> orderbook)
{
    clearConsole();
    std::cout << orderbook->getTransactionLog() << std::endl;
    system("pause");
}

int main() 
{
    std::shared_ptr<Orderbook> orderbook = std::make_shared<Orderbook>();
    clearConsole();

    Login_Screen();
    int choice;

    while (true)
    {
        std::cin >> choice;
        clearConsole();

        switch (choice)
        {
            case 1:
                orderbook->printVisual();
                system("pause");
                break;
            case 2:
                Print_TransactionLog(orderbook);
                break;
            case 3:
                int Input_ActionType;
                std::cout << "Enter Action:" << std::endl
                          << "1. Add" << std::endl
                          << "2. Modify" << std::endl
                          << "3. Cancel" << std::endl;
                std::cout << "Selection: ";
                std::cin >> Input_ActionType;
                clearConsole();

                if (Input_ActionType == 1)
                    Handle_Add(orderbook);
                else if (Input_ActionType == 2)
                    Handle_Modify(orderbook);
                else if (Input_ActionType == 3)
                    Handle_Cancel(orderbook);
                else
                    throw std::logic_error("Unsupported Type");

                break;
            default:
                return 0;
        }

        clearConsole();

        std::cout << "Please enter your option:\n";
        std::cout << "1. Print Orderbook\n"
                  << "2. Print Transaction Log\n"
                  << "3. Add Order\n"
                  << "4. Exit\n";
    }

    return 0;
}
