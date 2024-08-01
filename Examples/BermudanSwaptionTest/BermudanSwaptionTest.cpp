/*
My Test Project of Bermudan Swaption pricing
*/

#include <ql/qldefines.hpp>
#if !defined(BOOST_ALL_NO_LIB) && defined(BOOST_MSVC)
#  include <ql/auto_link.hpp>
#endif
// For swap definition
#include <ql/utilities/dataformatters.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/instruments/swaption.hpp>
// For model
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swaption/jamshidianswaptionengine.hpp>
#include <ql/pricingengines/swaption/fdhullwhiteswaptionengine.hpp>
// For calibration
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/calibrationhelper.hpp>

#include <iostream>
#include <iomanip>

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
        /* Calibration Approach: Co-terminal swap calibration at strike */
        /****************************************************************/

        // flat yield term structure impling 1x5 swaps at 5%
        auto flatRate = ext::make_shared<SimpleQuote>(0.04875825);
        Handle<YieldTermStructure> rhTermStructure(
            ext::make_shared<FlatForward>(
                settlementDate, Handle<Quote>(flatRate), Actual365Fixed()));

        // auto rhTermStructure2 = ext::make_shared<YieldTermStructure>(
        //     ext::make_shared<FlatForward>(
        //         settlementDate, ext::make_shared<Quote>(flatRate),
        //         Actual365Fixed()));
        
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
                                BlackCalibrationHelper::CalibrationErrorType::ImpliedVolError));
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
                  << "Use iterations: " << modelHW->functionEvaluation() << "\n";
        std::cout << "calibrated to:\n"
                  << "a = " << modelHW->params()[0] << ", "
                  << "sigma = " << modelHW->params()[1]
                  << std::endl << std::endl;

        return 0;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
    }
}