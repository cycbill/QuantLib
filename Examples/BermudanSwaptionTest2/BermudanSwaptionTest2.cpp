/*
My Test Project of Bermudan Swaption pricing
Test case: real rate curve term structure
*/

#include <ql/qldefines.hpp>
#if !defined(BOOST_ALL_NO_LIB) && defined(BOOST_MSVC)
#  include <ql/auto_link.hpp>
#endif
// For swap definition
#include <ql/utilities/dataformatters.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/indexes/ibor/eonia.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/termstructures/yield/oisratehelper.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/math/interpolations/cubicinterpolation.hpp>
// For model
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swaption/jamshidianswaptionengine.hpp>
#include <ql/pricingengines/swaption/fdhullwhiteswaptionengine.hpp>
// For calibration
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/calibrationhelper.hpp>
// For bond
#include <ql/instruments/bonds/zerocouponbond.hpp>
#include <ql/pricingengines/bond/discountingbondengine.hpp>

#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace QuantLib;


// Number of swaptions to be calibrated to...
Size numRows = 5;
Size numCols = 5;

// Define volatility term structure
Integer swapLengths[] = {
    1,     2,     3,     4,     5};
Volatility swaptionVols[] = {
0.1490, 0.1340, 0.1228, 0.1189, 0.1148,
0.1290, 0.1201, 0.1146, 0.1108, 0.1040,
0.1149, 0.1112, 0.1070, 0.1010, 0.0957,
0.1047, 0.1021, 0.0980, 0.0951, 0.1270,
0.1000, 0.0950, 0.0900, 0.1230, 0.1160};


void calibrateModel(
    const ext::shared_ptr<ShortRateModel>& model,
    const std::vector<ext::shared_ptr<BlackCalibrationHelper>>& swaptions)
{
    std::vector<ext::shared_ptr<CalibrationHelper>> helpers(swaptions.begin(), swaptions.end());
    LevenbergMarquardt optimizer;

    // Note: calibrationErrorType = RelativePriceError for swaptionHelper
    model->calibrate(helpers, optimizer, 
        EndCriteria(400, 100, 1.0e-8, 1.0e-8, 1.0e-8));

    // Output the implied Black volatilities
    for (Size i = 0; i < numRows; i++) {
        Size j = numCols - i - 1;
        Size k = i * numCols + j;
        Real modelValue= swaptions[i]->modelValue();
        Real marketValue = swaptions[i]->marketValue();
        Volatility implied = swaptions[i]->impliedVolatility(modelValue, 1e-4, 1000, 0.05, 0.50);
        Volatility diff = implied - swaptionVols[k];

        std::cout << i+1 << "x" << swapLengths[j]
                  << std::setprecision(5) << std::noshowpos
                  << ": model " << std::setw(7) << io::volatility(implied)
                  << ", market " << std::setw(7)
                  << io::volatility(swaptionVols[k])
                  << " (" << std::setw(7) << std::showpos
                  << io::volatility(diff) << std::noshowpos << "),  "
                  << "modelValue " << std::setprecision(9) << std::setw(10) << modelValue
                  << ", marketValue "<< std::setprecision(9) << std::setw(10) << marketValue << "\n";
    }
}


int main(int, char* []) {
    try {
        // Define dates
        Date todaysDate(15, Feb, 2002);
        Calendar calendar = TARGET();
        Date settlementDate(19, Feb, 2002);
        Settings::instance().evaluationDate() = todaysDate;

        /****************************************************************/
        /*************** Define rate curve term structure ***************/
        /****************************************************************/
        // RateHelpers are built from the quotes, together with
        // other info depending on the instrument.  Quotes are passed in
        // relinkable handles which could be relinked to some other
        // data source later.

        std::vector<ext::shared_ptr<RateHelper>> eoniaInstruments;

        // deposits
        // std::map<Period, ext::shared_ptr<Quote>> depoQuotes = {
        //     // settlement days, quote
        //     {3 * Months, ext::make_shared<SimpleQuote>(0.0517)},
        //     {6 * Months, ext::make_shared<SimpleQuote>(0.0484)},
        //     {1 * Years, ext::make_shared<SimpleQuote>(0.0436)},
        //     {2 * Years, ext::make_shared<SimpleQuote>(0.0388)},
        //     {5 * Years, ext::make_shared<SimpleQuote>(0.0362)},
        //     {10 * Years, ext::make_shared<SimpleQuote>(0.0379)},
        //     {30 * Years, ext::make_shared<SimpleQuote>(0.0411)}
        // };

        // DayCounter depositDayCounter = Actual365Fixed();

        // for (const auto& q : depoQuotes) {
        //     auto settlementDays = days(q.first);
        //     auto quote = q.second;
        //     auto helper = ext::make_shared<DepositRateHelper>(
        //         Handle<Quote>(quote),
        //         1 * Days, settlementDays,
        //         calendar, Following,
        //         false, depositDayCounter);
        //     eoniaInstruments.push_back(helper);
        // }

        // short-term OIS
        auto eonia = ext::make_shared<Eonia>();
        std::map<Period, ext::shared_ptr<Quote>> shortOisQuotes = {
            {3 * Months, ext::make_shared<SimpleQuote>(0.0517)},
            {6 * Months, ext::make_shared<SimpleQuote>(0.0484)},
            {1 * Years, ext::make_shared<SimpleQuote>(0.0436)},
            {2 * Years, ext::make_shared<SimpleQuote>(0.0388)},
            {5 * Years, ext::make_shared<SimpleQuote>(0.0362)},
            {10 * Years, ext::make_shared<SimpleQuote>(0.0379)},
            {30 * Years, ext::make_shared<SimpleQuote>(0.0411)}
        };

        for (const auto& q : shortOisQuotes) {
            auto tenor = q.first;
            auto quote = q.second;
            auto helper = ext::make_shared<OISRateHelper>(
                2, tenor, Handle<Quote>(quote), eonia);
            eoniaInstruments.push_back(helper);
        }

        // curve
        DayCounter termStructureDayCounter = Actual365Fixed();
        auto eoniaTermStructure = ext::make_shared<PiecewiseYieldCurve<Discount, Cubic>>(
                todaysDate, eoniaInstruments, termStructureDayCounter);

        Handle<YieldTermStructure> rhTermStructure(eoniaTermStructure);

        /****************************************************************/
        /* Calibration Approach: Co-terminal swap calibration at strike */
        /****************************************************************/

        // flat yield term structure impling 1x5 swaps at 5%
        // auto flatRate = ext::make_shared<SimpleQuote>(0.04875825);
        // Handle<YieldTermStructure> rhTermStructure(
        //     ext::make_shared<FlatForward>(
        //         settlementDate, Handle<Quote>(flatRate), Actual365Fixed()));
        
        // Define a swap
        Frequency fixedLegFrequency = Annual;
        Frequency floatingLegFrequency = Semiannual;
        BusinessDayConvention fixedLegConvention = Unadjusted;
        BusinessDayConvention floatingLegConvention = ModifiedFollowing;
        DayCounter fixedLegDayCounter = Thirty360(Thirty360::European);
        Swap::Type type = Swap::Payer;
        Rate dummyFixedRate = 0.03;
        auto indexSixMonths = ext::make_shared<Euribor6M>(rhTermStructure);

        Date startDate = calendar.advance(settlementDate, 1, Years, floatingLegConvention);
        Date maturity = calendar.advance(startDate, 5, Years, floatingLegConvention);
        Schedule fixedSchedule(startDate, maturity, Period(fixedLegFrequency), 
                               calendar, fixedLegConvention, fixedLegConvention,
                               DateGeneration::Forward, false);
        Schedule floatSchedule(startDate, maturity, Period(floatingLegFrequency), 
                               calendar, floatingLegConvention, floatingLegConvention,
                               DateGeneration::Forward, false);        

        auto swap = ext::make_shared<VanillaSwap>(
            type, 1000.0,
            fixedSchedule, dummyFixedRate, fixedLegDayCounter,
            floatSchedule, indexSixMonths, 0.0,
            indexSixMonths->dayCounter()
        );
        swap->setPricingEngine(ext::make_shared<DiscountingSwapEngine>(rhTermStructure));

        std::cout << "Swap with fixed rate = " << dummyFixedRate << std::endl;
        std::cout << "Price = " << swap->NPV() << std::endl;
        std::cout << "Fair rate = " << swap->fairRate() << std::endl;
        std::cout << "Fixed leg BPS = " << swap->fixedLegBPS() << ", float leg BPS = " << swap->floatingLegBPS() << std::endl;


        // Define the ATM/OTM/ITM swaps
        Rate fixedATMRate = swap->fairRate();
        Rate fixedOTMRate = fixedATMRate * 1.2;
        Rate fixedITMRate = fixedATMRate * 0.8;

        auto atmSwap = ext::make_shared<VanillaSwap>(
            type, 1000.0,
            fixedSchedule, fixedATMRate, fixedLegDayCounter,
            floatSchedule, indexSixMonths, 0.0,
            indexSixMonths->dayCounter());
        atmSwap->setPricingEngine(ext::make_shared<DiscountingSwapEngine>(rhTermStructure));
        auto otmSwap = ext::make_shared<VanillaSwap>(
            type, 1000.0,
            fixedSchedule, fixedOTMRate, fixedLegDayCounter,
            floatSchedule, indexSixMonths, 0.0,
            indexSixMonths->dayCounter());
        otmSwap->setPricingEngine(ext::make_shared<DiscountingSwapEngine>(rhTermStructure));
        auto itmSwap = ext::make_shared<VanillaSwap>(
            type, 1000.0,
            fixedSchedule, fixedITMRate, fixedLegDayCounter,
            floatSchedule, indexSixMonths, 0.0,
            indexSixMonths->dayCounter());
        itmSwap->setPricingEngine(ext::make_shared<DiscountingSwapEngine>(rhTermStructure));

        std::cout << "ATMSwap fixed rate = " << fixedATMRate
        << ", NPV = " << atmSwap->NPV() << std::endl;
        std::cout << "OTMSwap fixed rate = " << fixedOTMRate
        << ", NPV = " << otmSwap->NPV() << std::endl;
        std::cout << "ITMSwap fixed rate = " << fixedITMRate
        << ", NPV = " << itmSwap->NPV() << std::endl << std::endl;
        
        // define the swaptions used in model calibration
        std::vector<Period> swaptionMaturities;
        swaptionMaturities.emplace_back(1, Years);
        swaptionMaturities.emplace_back(2, Years);
        swaptionMaturities.emplace_back(3, Years);
        swaptionMaturities.emplace_back(4, Years);
        swaptionMaturities.emplace_back(5, Years);

        std::vector<ext::shared_ptr<BlackCalibrationHelper>> swaptions;

        // List of times that have to be included in the time grid
        std::list<Time> times;
        Size i;
        for (int i = 0; i < numRows; i++)
        {
            Size j = numCols - i - 1; // 1x5, 2x4, 3x3, 4x2, 5x1
            Size k = i * numCols + j;
            auto vol = ext::make_shared<SimpleQuote>(swaptionVols[k]);
            swaptions.push_back(ext::make_shared<SwaptionHelper>(
                                swaptionMaturities[i],
                                Period(swapLengths[j], Years),
                                Handle<Quote>(vol),
                                indexSixMonths,
                                indexSixMonths->tenor(),
                                indexSixMonths->dayCounter(),
                                indexSixMonths->dayCounter(),
                                rhTermStructure,
                                BlackCalibrationHelper::CalibrationErrorType::RelativePriceError));
            swaptions.back()->addTimesTo(times);
        }

        // Define the models
        auto modelHW = ext::make_shared<HullWhite>(rhTermStructure);

        // Model calibration
        std::cout << "Hull-White (analytic formulae) calibration" << std::endl;
        for (i=0; i<swaptions.size(); i++)
            swaptions[i]->setPricingEngine(ext::make_shared<JamshidianSwaptionEngine>(modelHW));

        calibrateModel(modelHW, swaptions);

        std::cout << "problemValues: " << modelHW->problemValues()
                  << ", use iterations: " << modelHW->functionEvaluation() << "\n";
        std::cout << "calibrated to:\n"
                  << "a = " << modelHW->params()[0] << ", "
                  << "sigma = " << modelHW->params()[1]
                  << std::endl << std::endl;

        // Bond prices from HW
        Real bondPriceHW = modelHW->discount((maturity - todaysDate)/365.);
        std::cout << "\nBond price from HW model: " << bondPriceHW << std::endl;
        Real bondPrice = rhTermStructure->discount(maturity);
        std::cout << "Bond price from rate curve: " << bondPrice << std::endl;
        
        // Generate curve term structure
        for(const auto& quotePair : shortOisQuotes)
        {
            Date curveMat = calendar.advance(todaysDate, quotePair.first, floatingLegConvention);
            bondPriceHW = modelHW->discount((curveMat - todaysDate)/365.);
            bondPrice = rhTermStructure->discount(curveMat);
            std::cout << "Curve mat " << quotePair.first 
            << ": Mkt bond price = " << bondPrice
            << ", HW bond price = " << bondPriceHW << std::endl;
        }


        return 0;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
    }
}