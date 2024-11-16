This Order Book system simulates a trading platform where Buy and Sell orders are matched against each other based on price and quantity. The key features include:

Order Types and Structure:

Order class encapsulates information about each order, including the order ID, price, side (buy/sell), quantity, and order type (e.g., GoodTillCancel, FillAndKill).
Orders are tracked and managed using an unordered_map that maps OrderId to OrderPointer, representing the order and its location in the system.
Order Book Levels:

The system uses two primary levels for managing orders:
Bids: The buy side of the order book, where orders are sorted in descending order of price.
Asks: The sell side, where orders are sorted in ascending order of price.
These levels are implemented using maps: one for bids and one for asks, both sorted by price to efficiently match orders.
Order Matching:

Matching logic: The order book matches orders if the price of the bid is greater than or equal to the price of the ask (for buy/sell orders). When a match is found, the quantity of the orders is compared, and the order with the smaller quantity is filled first.
After a match, the order's remaining quantity is updated, and the order is either filled completely or partially. If an order is completely filled, it is removed from the book.
Trade Generation:

When orders are matched, a Trade is generated, which records the OrderId, Price, and Quantity of the matched Bid and Ask orders.
Order Modification and Cancellation:

Orders can be modified or canceled. If an order is modified (e.g., price or quantity), it is canceled, and the new modified order is added to the book.
Cancellations remove an order from the book without performing any trade.
Transaction Information:

The system tracks all trades through the Trade class, which stores the details of both the bid and ask sides of each transaction.
