/*
My Test Project of Bermudan Swaption pricing
*/

#include <ql/qldefines.hpp>
#if !defined(BOOST_ALL_NO_LIB) && defined(BOOST_MSVC)
#  include <ql/auto_link.hpp>
#endif
#include <ql/utilities/dataformatters.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/instruments/swaption.hpp>

#include <iostream>
#include <iomanip>

using namespace QuantLib;

Size numRows = 5;
Size numCols = 5;

//Number of swaptions to be calibrated to...

Integer swapLengths[] = {
      1,     2,     3,     4,     5};
Volatility swaptionVols[] = {
  0.1490, 0.1340, 0.1228, 0.1189, 0.1148,
  0.1290, 0.1201, 0.1146, 0.1108, 0.1040,
  0.1149, 0.1112, 0.1070, 0.1010, 0.0957,
  0.1047, 0.1021, 0.0980, 0.0951, 0.1270,
  0.1000, 0.0950, 0.0900, 0.1230, 0.1160};


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

        auto rhTermStructure2 = ext::make_shared<YieldTermStructure>(
            ext::make_shared<FlatForward>(
                settlementDate, ext::make_shared<Quote>(flatRate),
                Actual365Fixed()));
        
        // Define the ATM/OTM/ITM swaps
        Frequency fixedLegFrequency = Annual;
        Frequency floatingLegFrequency = Semiannual;
        BusinessDayConvention fixedLegConvention = Unadjusted;
        BusinessDayConvention floatingLegConvention = ModifiedFollowing;
        DayCounter fixedLegDayCounter = Thirty360(Thirty360::European);



        return 0;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
    }
}