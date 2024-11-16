
#include <iostream>

enum class OrderType {
    GoodTillCancel,
    FillAndKill,
};

enum class Side {
    Buy, 
    Sell,
};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;

struct LevelInfo {
    Price price;
    Quantity quantity;
};

using LevelInfoVector = std::vector<LevelInfo>;

class OrderBookLevelInfo {
private:
    LevelInfo m_bids;
    LevelInfo m_asks;
public:
    OrderBookLevelInfo(const LevelInfo& bids, const LevelInfo& asks)
        : m_bids{bids}, m_asks{asks} {}

    const LevelInfo& getBids() const { return m_bids; }
    const LevelInfo& getAsks() const { return m_asks; }
};

class Order {
private:
    OrderType m_orderType;
    OrderId m_orderId;
    Side m_side;
    Price m_price;
    Quantity m_initialQuantity;
    Quantity m_remainingQuantity;
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : m_orderType(orderType), m_orderId(orderId), m_side(side), m_price(price),
          m_initialQuantity(quantity), m_remainingQuantity(quantity) {}

    OrderId getOrderId() const { return m_orderId; }
    Side getSide() const { return m_side; }
    Price getPrice() const { return m_price; }
    OrderType getOrderType() const { return m_orderType; }
    Quantity getInitialQuantity() const { return m_initialQuantity; }
    Quantity getRemainingQuantity() const { return m_remainingQuantity; }
    Quantity getFilledQuantity() const { return getInitialQuantity() - getRemainingQuantity(); }
    bool isFilled() const { return getRemainingQuantity() == 0; }

    void Fill(Quantity quantity) {
        if (quantity > getRemainingQuantity()) {
            throw std::logic_error("Cannot fill order for more than remaining quantity.");
        }
        m_remainingQuantity -= quantity; 
    }
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify {
private:
    OrderId m_orderId;
    Price m_price;
    Side m_side;
    Quantity m_quantity;
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
        : m_orderId(orderId), m_price(price), m_side(side), m_quantity(quantity) {}

    OrderId getOrderId() const { return m_orderId; }
    Price getPrice() const { return m_price; }
    Side getSide() const { return m_side; }
    Quantity getQuantity() const { return m_quantity; }

    OrderPointer ToOrderPointer(OrderType type) const {
        return std::make_shared<Order>(type, getOrderId(), getSide(), getPrice(), getQuantity());
    }
};

struct TradeInfo {
    OrderId m_orderId;
    Price m_price;
    Quantity m_quantity;
};

class Trade {
private:
    TradeInfo m_bidTrade;
    TradeInfo m_askTrade;
public:
    Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
        : m_bidTrade(bidTrade), m_askTrade(askTrade) {}

    const TradeInfo& getBidTrade() const { return m_bidTrade; }
    const TradeInfo& getAskTrade() const { return m_askTrade; }
};

using Trades = std::vector<Trade>;

class OrderBook {
private:
    struct OrderEntry {
        OrderPointer m_order = nullptr;
        OrderPointers::iterator m_location;
    };

    std::map<Price, OrderPointers, std::greater<Price>> bids;
    std::map<Price, OrderPointers, std::less<Price>> asks;
    std::unordered_map<OrderId, OrderEntry> orders;

    bool canMatch(Side side, Price price) const {
        if (side == Side::Buy) {
            if (asks.empty()) {
                return false;
            }
            const auto& [bestAsk, value] = *asks.begin();
            return price >= bestAsk;
        } else {
            if (bids.empty()) {
                return false;
            }
            const auto& [bestBid, value] = *bids.begin();
            return price <= bestBid;
        }
    }

    Trades matchOrders() {
        Trades trades;
        while (!bids.empty() && !asks.empty()) {
            auto& [bidPrice, bidOrders] = *bids.begin();
            auto& [askPrice, askOrders] = *asks.begin();

            if (bidPrice < askPrice) {
                break;
            }

            while (!bidOrders.empty() && !askOrders.empty()) {
                auto& bid = bidOrders.front();
                auto& ask = askOrders.front();
                Quantity quantity = std::min(bid->getRemainingQuantity(), ask->getRemainingQuantity());

                bid->Fill(quantity);
                ask->Fill(quantity);

                if (bid->isFilled()) {
                    bidOrders.pop_front();
                    orders.erase(bid->getOrderId());
                }
                if (ask->isFilled()) {
                    askOrders.pop_front();
                    orders.erase(ask->getOrderId());
                }

                if (bidOrders.empty()) {
                    bids.erase(bidPrice);
                }
                if (askOrders.empty()) {
                    asks.erase(askPrice);
                }

                trades.push_back(Trade{
                    TradeInfo{bid->getOrderId(), bid->getPrice(), quantity},
                    TradeInfo{ask->getOrderId(), ask->getPrice(), quantity}
                });
            }
        }
        return trades;
    }

public:
    Trades addOrder(OrderPointer order) {
        if (orders.contains(order->getOrderId())) {
            return {};  // Order already exists
        }

        if (order->getOrderType() == OrderType::FillAndKill && !canMatch(order->getSide(), order->getPrice())) {
            return {};  // No match found for FillAndKill order
        }

        OrderPointers::iterator iterator;
        if (order->getSide() == Side::Buy) {
            auto& orderList = bids[order->getPrice()];
            orderList.push_back(order);
            iterator = std::prev(orderList.end());
        } else {
            auto& orderList = asks[order->getPrice()];
            orderList.push_back(order);
            iterator = std::prev(orderList.end());
        }

        orders.insert({order->getOrderId(), OrderEntry{order, iterator}});
        return matchOrders();
    }

    void cancelOrder(OrderId orderId) {
        if (!orders.contains(orderId)) {
            return;  // Order not found
        }

        const auto& [order, orderIterator] = orders.at(orderId);
        orders.erase(orderId);

        if (order->getSide() == Side::Buy) {
            auto& orderList = bids.at(order->getPrice());
            orderList.erase(orderIterator);
            if (orderList.empty()) {
                bids.erase(order->getPrice());
            }
        } else {
            auto& orderList = asks.at(order->getPrice());
            orderList.erase(orderIterator);
            if (orderList.empty()) {
                asks.erase(order->getPrice());
            }
        }
    }

    Trades matchOrder(OrderModify order) {
        if (!orders.contains(order.getOrderId())) {
            return {};  // Order not found
        }

        const auto& [existingOrder, value] = orders.at(order.getOrderId());
        cancelOrder(order.getOrderId());
        return addOrder(order.ToOrderPointer(existingOrder->getOrderType()));
    }

    std::size_t Size() const { return orders.size(); }

    OrderBookLevelInfo getOrderInfo() const {
        LevelInfo bidInfo, askInfo;
        auto createLevelInfo = [](Price price, const OrderPointers& orders) {
            return LevelInfo{
                price,
                std::accumulate(orders.begin(), orders.end(), (Quantity)0,
                    [] (Quantity runningSum, const OrderPointer& order) {
                        return runningSum + order->getRemainingQuantity();
                    }
                )
            };
        };

        for (const auto& [price, orders] : bids) {
            bidInfo = createLevelInfo(price, orders);
        }
        for (const auto& [price, orders] : asks) {
            askInfo = createLevelInfo(price, orders);
        }

        return OrderBookLevelInfo(bidInfo, askInfo);
    }
};

int main() {
    OrderBook orderbook;
    const OrderId orderId = 1;
    orderbook.addOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId, Side::Buy, 100, 10));
    cout << "Order book size is: " << orderbook.Size() << '\n';
    orderbook.cancelOrder(orderId);
    cout << "Order book size is: " << orderbook.Size() << '\n';
}
