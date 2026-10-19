#pragma once

namespace quant {

// Interface so alternative fee schedules can be swapped in without touching the Portfolio.
class CostModel {
public:
    virtual ~CostModel() = default;
    virtual double feeRate() const = 0;       // fraction of traded notional, per side
    virtual double slippageRate() const = 0;  // fraction of price, per side
    double fee(double notional) const { return feeRate() * (notional < 0 ? -notional : notional); }
};

// Fee and slippage in basis points (1 bp = 0.01%).
class BpsCostModel : public CostModel {
public:
    BpsCostModel(double feeBps, double slippageBps) : fee_(feeBps / 1e4), slip_(slippageBps / 1e4) {}
    double feeRate() const override { return fee_; }
    double slippageRate() const override { return slip_; }

private:
    double fee_;
    double slip_;
};

}  // namespace quant
