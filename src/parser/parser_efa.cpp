/****************************************************************************
**
**  This file is a part of Fahrplan.
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License along
**  with this program.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/

/* Overview
 *  This parser was written by Evan Sebire
 *  Should work with both EFAv9 and v10
 *  Information regarding EFA can be found at the following links:
 *  main webpage from Mentz Datenverarbeitung GmbH(Munich, 02, DE), http://www.mentzdv.de/englisch/products/efa/
 *  good explaination of XML; http://data.linz.gv.at/katalog/linz_ag/linz_ag_linien/fahrplan/LINZ_LINIEN_Schnittstelle_EFA_V1.pdf
 *  http://www.tfl.gov.uk/assets/downloads/businessandpartners/journey-planner-api-documentation.pdf.pdf    Still refers to EFAv9 not v10
 *  documentation for the london deployment, https://code.google.com/p/openefa/
 *  http://patrickbrosi.de/?q=simpleefa#q=simpleefa
 *
 *
 * Transport providers that use EFA
 *  Austria
 *  http://efa.vvt.at/vvtadr/   VVT
 *  http://efa.vor.at/wvb/
 *  http://efa.svv-info.at/svv/
 *  http://fahrplan.verbundlinie.at/stv/
 *  http://efaneu.vmobil.at/vvvmobile/      Verkehrsverbund Vorarlberg
 *  http://www.linzag.at/linz2/
 *  http://efa.ivb.at/ivb/
 *
 *  Australian
 *  Melbourne: http://jp.ptv.vic.gov.au/ptv/
 *  Sydney: http://mobile.131500.com.au/TripPlanner/mobile/     changed to v10 in Jan 2014
 *
 *  England
 *  http://www.travelinemidlands.co.uk/wmtis/
 *  http://www.travelineeastmidlands.co.uk/em/
 *  http://www.travelinesw.com/swe/XSLT_REQUEST?  Southern England and midlands
 *  London; http://journeyplanner.tfl.gov.uk/user/XSLT_TRIP_REQUEST2?language=en    EFA 10
 *
 *  German
 *  http://www.kvv.de/tunnelEfaDirect.php?action=XML_COORD_REQUEST  XML_GEOOBJECT_REQUEST   EFAv10
 *  http://efa.naldo.de/naldo/
 *  http://www2.vvs.de/vvs/
 *  http://efa.vvo-online.de:8080/dvb/
 *  http://efa.mobilitaetsverbund.de/web/
 *  http://efa9.vrn.de/vrt/
 *  http://app.vrr.de/standard/
 *  http://fahrplanauskunft.vrn.de/vrn_mobile/
 *  http://www.vms.de/vms2/     VMS
 *  http://www.efa-bw.de/nvbw/
 *  http://efa.mvv-muenchen.de/mobile/
 *  http://mobil.mvg-online.de/
 *  http://efa.vag-freiburg.de/vagfr/
 *  http://mobil.efa.de/mobile3/    GVH
 *  Donau-Iller-Nahverkehrsverbund-GmbH;    http://www.ding.eu/fahrplaene/fahrplanauskunft.html
 *  http://195.30.98.162:8081/vvv2/     Verkehrsverbund Vogtland (VVV), In West-Sachsen

 *
 *  Ireland
 *  http://www.journeyplanner.transportforireland.ie/nta/      EFA 10
 *
 *  Italian
 *  Bologna: http://tpweb.atc.bo.it/atc2/    ATC
 *
 *  Slovenia
 *  Maribor: http://164.8.32.183/slo/
 *
 *  Swiss
 *  http://www.efa-bvb.ch/bvb/
 *  http://mobil.vbl.ch/vblmobil/
 *
 *  UAE
 *  http://wojhati.rta.ae/dub/
 *
 *  US
 *  Chicago; http://www.transitchicago.com/   Chicago Transit Authority, also known as CTA, can't find public access
 *  San Francisco, Bay area; http://tripplanner.transit.511.org/mtc/XSLT_TRIP_REQUEST2?language=en
 */


#include "parser_efa.h"

#include <QBuffer>
#include <QFile>
#include <QNetworkReply>
#include <QtCore/QUrl>
#if defined(BUILD_FOR_QT5)
    #include <QUrlQuery>
#endif

QHash<QString, JourneyDetailResultList *> cachedJourneyDetailsEfa;

#define getAttribute(node, key) (node.attributes().namedItem(key).toAttr().value())

ParserEFA::ParserEFA(QObject *parent) :
    ParserAbstract(parent){

    m_searchJourneyParameters.isValid = false;
    m_timeTableForStationParameters.isValid = false;
}

bool ParserEFA::supportsGps()
{
    return true;
}

bool ParserEFA::supportsVia()
{
    return true;
}


bool ParserEFA::supportsTimeTable()
{
    return true;
}

bool ParserEFA::supportsTimeTableDirection()
{
    return false;
}

QStringList ParserEFA::getTrainRestrictions()
{
    QStringList result;
    result.append(tr("All"));
    result.append(tr("Metro Train"));
    result.append(tr("Metro Tram"));
    result.append(tr("Metro Bus"));
    result.append(tr("Regional Train"));
    result.append(tr("Regional Coach"));
    result.append(tr("Airport(SKYBUS)"));
    return result;
}

void ParserEFA::findStationsByName(const QString &stationName)
{
    // http://jp.ptv.vic.gov.au/ptv/XML_STOPFINDER_REQUEST?locationServerActive=1&outputFormat=XML&type_sf=any&name_sf=lilydale
    //http://www.journeyplanner.transportforireland.ie/nta/XML_DM_REQUEST?language=en&type_sf=any&type_dm=any&coordOutputFormat=WGS84&name_dm=cork&name_sf=cork&itdDateDay=26&itDateYearMonth=201309&itdTimeHour=09&itdTimeMinute=48&itdTripDateTimeDepArr=dep&deleteAssignedStops_dm=1&useRealtime=1&mode=direct

    qDebug() << "ParserEFA::findStationsByName(" <<  stationName << ")";
    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    currentRequestState = FahrplanNS::stationsByNameRequest;

    QUrl uri(baseRestUrl + "XML_STOPFINDER_REQUEST");

#if defined(BUILD_FOR_QT5)
    QUrlQuery query;
#else
    QUrl query;
#endif
    query.addQueryItem("language", "en");
    query.addQueryItem("locationServerActive", "1");
    query.addQueryItem("outputFormat", "XML");
    query.addQueryItem("type_sf", "stop");  // could be any, poi or stop
    query.addQueryItem("coordOutputFormat","WGS84");
    //<input name="locality_origin" id="locality_origin" value="Cork" type="hidden">
    query.addQueryItem("name_sf", stationName);
#if defined(BUILD_FOR_QT5)
    uri.setQuery(query);
#else
    uri.setQueryItems(query.queryItems());
#endif
    sendHttpRequest(uri);


    qDebug() << "search station url:" << uri;
}

void ParserEFA::getTimeTableForStation(const Station &currentStation, const Station &, const QDateTime &dateTime, Mode mode, int)
{
    qDebug() << "void ParserEFA::getTimeTableForStation(" << currentStation.name << dateTime;

    // http://journeyplanner.tfl.gov.uk/user/XML_DM_REQUEST?language=en&sessionID=0&ptOptionsActive=&itdLPxx_tubeMap=&itdLPxx_request=&command=&lsShowTrainsExplicit=1&name_dm=1001180&nameState_dm=notidentified&place_dm=London&type_dm=stopID
    if (currentRequestState != FahrplanNS::noneRequest)
        return;
    currentRequestState = FahrplanNS::getTimeTableForStationRequest;

    m_timeTableForStationParameters.isValid = false;

    QUrl uri(baseRestUrl + "XML_DM_REQUEST");

#if defined(BUILD_FOR_QT5)
    QUrlQuery query;
#else
    QUrl query;
#endif
    //query.addQueryItem("typeInfo_dm", "stopID");                      typeInfo_dm and type_dm seem to have the same effect
    query.addQueryItem("type_dm", "stopID");
    query.addQueryItem("name_dm", currentStation.id.toString());  //    nameInfo_dm and name_dm seem to be the same
    query.addQueryItem("coordOutputFormat","WGS84");
    query.addQueryItem("itdDateDay", dateTime.toString("dd"));
    query.addQueryItem("itdDateYearMonth", dateTime.toString("yyyyMM"));
    query.addQueryItem("itdTimeHour", dateTime.toString("hh"));
    query.addQueryItem("itdTimeMinute", dateTime.toString("mm"));
    if(mode == Departure)
        query.addQueryItem("itdTripDateTimeDepArr", "dep");
    else
        query.addQueryItem("itdTripDateTimeDepArr", "arr");
    //query.addQueryItem("deleteAssignedStops_dm", "1");                not sure what this does
    query.addQueryItem("useRealtime", "1");                         // probably best to display realtime not scheduled time
    query.addQueryItem("depType", "STOPEVENTS");
    //query.addQueryItem("nameState_dm", "notidentified");
    //query.addQueryItem("lsShowTrainsExplicit", "1");
    //query.addQueryItem("place_dm", "");
    query.addQueryItem("mode", "direct");

#if defined(BUILD_FOR_QT5)
    uri.setQuery(query);
#else
    uri.setQueryItems(query.queryItems());
#endif
    sendHttpRequest(uri);
}

void ParserEFA::findStationsByCoordinates(qreal longitude, qreal latitude)
{
    qDebug() << "ParserEFA::findStationsByCoordinates(longitude=" << longitude << ", latitude=" << latitude << ")";

    /* With EFA it's possible to send latitude and longitude and specify WGS84;
     *http://jp.ptv.vic.gov.au/ptv/XML_TRIP_REQUEST2?type_origin=coord&name_origin=-37.75587,145.347519:WGS84
     */

    if (currentRequestState != FahrplanNS::noneRequest)
        return;
    currentRequestState = FahrplanNS::stationsByCoordinatesRequest;



    QUrl uri(baseRestUrl + QLatin1String("XML_TRIP_REQUEST2"));
#if defined(BUILD_FOR_QT5)
    QUrlQuery query;
#else
    QUrl query;
#endif
    query.addQueryItem("type_origin", "coord");
    query.addQueryItem("name_origin", QString("%1:%2:WGS84").arg(longitude, 2, 'f', 6).arg(latitude, 2, 'f', 6));
    query.addQueryItem("coordListOutputFormat","STRING");
    query.addQueryItem("max","10");
    query.addQueryItem("inclFilter","1");
    query.addQueryItem("radius_1","3000");
    query.addQueryItem("type_1","STOP");     // ENTRANCE, BUS_POINT, POI_POINT
    query.addQueryItem("coordOutputFormat","WGS84");

#if defined(BUILD_FOR_QT5)
    uri.setQuery(query);
#else
    uri.setQueryItems(query.queryItems());
#endif
    sendHttpRequest(uri);
}

void ParserEFA::parseStationsByCoordinates(QNetworkReply *networkReply)
{
    qDebug() << "ParserEFA::parseStationsByCoordinates(networkReply.url()=" << networkReply->url().toString() << ")";
    StationsList result;

    QDomDocument doc("result");
    QByteArray data = readNetworkReply(networkReply);
    if (doc.setContent(data, false)) {
        QDomNodeList nodeList = doc.elementsByTagName("itdOdvAssignedStop");
        for (int i = 0; i < nodeList.size(); ++i) {
            QDomNode node = nodeList.item(i);
            Station item;

            QString value = getAttribute(node, "value");
            item.name=value.section(":",1,-1);
            item.id=value.section(":",0,0);
            item.type = "STATION";
            item.latitude = getAttribute(node, "x").toDouble();
            item.longitude = getAttribute(node, "y").toDouble();
            item.miscInfo = getAttribute(node, "distance") + "m";

            result << item;
        }
        checkForError(&doc);
    }

    emit stationsResult(result);
}

void ParserEFA::checkForError(QDomDocument *serverReplyDomDoc)
{
    QDomNodeList errorNodeList = serverReplyDomDoc->elementsByTagName("itdMessage");
    for(int i = 0; i < errorNodeList.size(); ++i) {
        QDomNode node = errorNodeList.item(i);
        QString error = getAttribute(node, "type");
        int code = getAttribute(node, "code").toInt();
        switch (code) {
        case -8011: // (unknown error)
        case -8012: // empty query
        case -8020: // no results
            continue;
        }

        if(error == "error" && code < 0) {
            QString errorText = node.toElement().text();
            if(errorText.length() < 1)
                errorText = QString::number(code);
            qDebug() << "Server Query Error:" << errorText << code;
            emit errorOccured(tr("Server Error: ") + errorText);
        }
    }
}

void ParserEFA::parseStationsByName(QNetworkReply *networkReply)
{
    qDebug() << "ParserEFA::parseStationsByName(networkReply.url()=" << networkReply->url().toString() << ")";

    StationsList result;
    QDomDocument doc("result");

    QByteArray data = readNetworkReply(networkReply);
    if (doc.setContent(data, false)) {

        //Check for error: <itdMessage type="error" module="BROKER" code="-2000">stop invalid</itdMessage>
        QDomNodeList errorNodeList = doc.elementsByTagName("itdMessage");
        for(int i = 0; i < errorNodeList.size(); ++i) {
            QDomNode node = errorNodeList.item(i);
            QString error = getAttribute(node, "type");
            if(error == "error")
            {
                qDebug() << "Query Error:" << node.toElement().text();
            }
        }

        QDomNode requestInfo = doc.elementsByTagName("itdRequest").item(0);
        int version = getAttribute(requestInfo, "version").section(".",0,0).toInt();

        qDebug() << "EFA version:" << version << ", complete version:" << getAttribute(requestInfo, "version");
        if(version < 10) {
            QDomNodeList nodeList = doc.elementsByTagName("odvNameElem");
            QDomNodeList modeNodeList = doc.elementsByTagName("itdStopModes");

            QStringList idList;

            for (int i = 0; i < nodeList.size(); ++i) {
                QDomNode node = nodeList.item(i);
                Station item;

                item.name = getAttribute(node, "objectName");
                item.id = getAttribute(node, "id");
                idList.append(item.id.toString());
                item.latitude = getAttribute(node, "x").toDouble();
                item.longitude = getAttribute(node, "y").toDouble();
                //item.type = getAttribute(modeNode, "mode");

                result << item;
            }

            for(int i = 0; i < modeNodeList.size(); ++i) {
                QDomNode modeNode = modeNodeList.item(i);
                QString id = getAttribute(modeNode, "id");

                int idIndex = idList.indexOf(id);
                if(idIndex > -1)
                    result[idIndex].type = getAttribute(modeNode, "mode");

            }
        }
        else {
            // London, Ireland, Sydney
            QDomNodeList nodeList = doc.elementsByTagName("odvNameElem");
            for(int i = 0; i < nodeList.size(); ++i) {
                QDomNode node = nodeList.item(i);
                Station item;
                item.name = node.toElement().text();
                item.id = getAttribute(node, "stopID");
                if(item.id.isNull())
                    item.id = getAttribute(node, "id");
                item.latitude = getAttribute(node, "x").toDouble();
                item.longitude = getAttribute(node, "y").toDouble();
                //item.type = getAttribute(modeNode, "mode");

                result << item;
            }
        }
        checkForError(&doc);
    }

    if (m_timeTableForStationParameters.isValid) {
        getTimeTableForStation(m_timeTableForStationParameters.currentStation, m_timeTableForStationParameters.directionStation, m_timeTableForStationParameters.dateTime, m_timeTableForStationParameters.mode, m_timeTableForStationParameters.trainrestrictions);
    } else if (m_searchJourneyParameters.isValid) {
        searchJourney(m_searchJourneyParameters.departureStation, m_searchJourneyParameters.arrivalStation, m_searchJourneyParameters.viaStation, m_searchJourneyParameters.dateTime, m_searchJourneyParameters.mode, m_searchJourneyParameters.trainrestrictions);
    } else {
        emit stationsResult(result);
    }
}

void ParserEFA::searchJourney(const Station &departureStation, const Station &viaStation, const Station &arrivalStation, const QDateTime &dateTime, ParserAbstract::Mode mode, int trainrestrictions)
{
    qDebug() << "ParserEFA::searchJourney(" << departureStation.name << viaStation.name << arrivalStation.name << dateTime << ")";

    if (currentRequestState != FahrplanNS::noneRequest)
        return;
    currentRequestState = FahrplanNS::searchJourneyRequest;

    m_searchJourneyParameters.isValid = false;
    m_searchJourneyParameters.departureStation = departureStation;
    m_searchJourneyParameters.arrivalStation = arrivalStation;
    m_searchJourneyParameters.viaStation = viaStation;
    m_searchJourneyParameters.dateTime = dateTime;
    m_searchJourneyParameters.mode = mode;
    m_searchJourneyParameters.trainrestrictions = trainrestrictions;


    currentRequestState = FahrplanNS::searchJourneyRequest;
    Q_UNUSED(viaStation);
    QString modeString = "dep";
    if (mode == Arrival) {
        modeString = "arr";
    }

    QUrl uri(baseRestUrl + QLatin1String("XML_TRIP_REQUEST2"));
#if defined(BUILD_FOR_QT5)
    QUrlQuery query;
#else
    QUrl query;
#endif
    query.addQueryItem("type_origin", "stop");
    query.addQueryItem("name_origin", departureStation.id.toString());
    query.addQueryItem("type_destination","stop");
    query.addQueryItem("name_destination",arrivalStation.id.toString());
    if(mode == 0)
        query.addQueryItem("itdTripDateTimeDepArr","dep");
    else
        query.addQueryItem("itdTripDateTimeDepArr","arr");
    query.addQueryItem("itdDateDay",dateTime.toString("dd"));
    query.addQueryItem("itdDateMonth",dateTime.toString("MM"));
    query.addQueryItem("itdDateYear",dateTime.toString("yyyy"));
    query.addQueryItem("itdTimeMinute",dateTime.toString("mm"));
    query.addQueryItem("itdTimeHour",dateTime.toString("hh"));
    query.addQueryItem("calcNumberOfTrips","5");
    query.addQueryItem("language","en");
    query.addQueryItem("coordOutputFormat","WGS84");
    query.addQueryItem("coordListOutputFormat","STRING");
    query.addQueryItem("coordOutputFormatTail","0");
    query.addQueryItem("useProxFootSearch","1");
    query.addQueryItem("itOptionsActive","1");
    query.addQueryItem("ptOptionsActive","1");


    /*  Options that should be used
     *  itOptionsActive=1 : enable individual transport options, possibly for preference quickest way (foot/bicycle)
     *  ptOptionsActive=1 : enable public transport options
     *  useProxFootSearch=1 : walk if its quicker
     *  changeSpeed = WALKSPEED      'slow'(200) || 'normal'(100) || 'fast'(50)
     *  As an alternative to the alias-names, values between 25 and 400 can be entered.
     *  The parameter value represents a kind of factor that modifies the default speed in the
     *  configuration file of the Journey Planner engine.
     *  The following formula is used: speed [km/h] = (100 * speed) / value of parameter
     *  Note: To use this parameter, you must have the options for public transport
     *  ptOptionsActive=1, the options for the individual transport itOptionsActive = 1 or
     *  both, enabled
     *
     *  if accessibility == BARRIER_FREE
     *     imparedOptionsActive=1  && wheelchair=on && noSolidStairs=on
     *  else if accessibility LIMITED
     *     imparedOptionsActive=1 && wheelchair=on && lowPlatformVhcl=on && noSolidStairs=on
     *
     *  nextDepsPerLeg=1     next departure if previous was missed
     *
     *  includedMeans
     *  inclMOT_0=on    HIGH SPEED TRAINS
     *  inclMOT_1=on    SUBURBAN_TRAIN
     *  inclMOT_2=on    SUBWAY
     *  inclMOT_3=on    Light rail
     *  inclMOT_4=on    Tram
     *  inclMOT_5=on    Metro Bus
     *  inclMOT_6=on    Regional Bus
     *  inclMOT_7=on    Express Bus
     *  inclMOT_8=on    Cable Car and Finicular
     *  inclMOT_9=on    Ferry
     *  inclMOT_10=on   Telebus / arrange pickup
     *  inclMOT_11=on   misc / other / Replacement Buses
     *  lineRestriction=403     all but ice?    not sure how this works outside DE
     *
     *  useRealtime=1 : returns actual dep/arr time and schedule time, latenesss can be calculated
     *  locationServerActive=1
     *  bikeTakeAlong=1 : possibility of taking bicycle
     *
     *  itdLPxx_view=detail : get detailed results
     */


#if defined(BUILD_FOR_QT5)
    uri.setQuery(query);
#else
    uri.setQueryItems(query.queryItems());
#endif
    sendHttpRequest(uri);

    qDebug() << "query url:" << uri;

}

void ParserEFA::parseSearchJourney(QNetworkReply *networkReply)
{
    qDebug() << "ParserEFA::parseSearchJourney(QNetworkReply *networkReply)";
    lastJourneyResultList = new JourneyResultList();

    for (QHash<QString, JourneyDetailResultList *>::Iterator it = cachedJourneyDetailsEfa.begin(); it != cachedJourneyDetailsEfa.end();) {
        JourneyDetailResultList *jdrl = it.value();
        it = cachedJourneyDetailsEfa.erase(it);
        delete jdrl;
    }

    /// Use fallback values for empty results (i.e. no connections found)
    lastJourneyResultList->setDepartureStation(m_searchJourneyParameters.departureStation.name);
    lastJourneyResultList->setViaStation(m_searchJourneyParameters.viaStation.name);
    lastJourneyResultList->setArrivalStation(m_searchJourneyParameters.arrivalStation.name);
    //: DATE, TIME
    lastJourneyResultList->setTimeInfo(tr("%1, %2", "DATE, TIME").arg(m_searchJourneyParameters.dateTime.date().toString(Qt::DefaultLocaleShortDate)).arg(m_searchJourneyParameters.dateTime.time().toString(Qt::DefaultLocaleShortDate)));

    m_earliestArrival = m_latestResultDeparture = QDateTime();

    QDomDocument doc("mydocument");
    //(const QString & text, QString * errorMsg = 0, int * errorLine = 0, int * errorColumn = 0)
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;
    QByteArray data = readNetworkReply(networkReply);
    doc.setContent(data, &errorMsg, &errorLine, &errorColumn);
    //qDebug() << "errorMsg:" << errorMsg << ", errorLine:" << errorLine << ", errorColumn:" << errorColumn;
    QDomNodeList routeList = doc.elementsByTagName("itdRoute");

    int numberOfChanges = 0;
    QString duration;

    for(int nodeCounter = 0 ; nodeCounter < routeList.size() ; ++nodeCounter)  {
        /* Each node has the following attributes: "vehicleTime" "alternative" "method" "print" "individualDuration" "publicDuration" "active" "distance" "routeIndex" "cTime" "selected" "searchMode" "delete" "changes" */
        JourneyResultItem *item = new JourneyResultItem();
        JourneyDetailResultList *detailsList = new JourneyDetailResultList();

        numberOfChanges = routeList.at(nodeCounter).toElement().attribute("changes").toInt();
        duration = routeList.at(nodeCounter).toElement().attribute("publicDuration");

        QDateTime departureDateTime;
        QDateTime arrivalDateTime;
        QList <QDateTime> departureTimesList;
        QList <QDateTime> arrivalTimesList;
        QStringList stationNameList;
        QStringList platformNameList;
        QStringList meansOfTransportNameList;
        QStringList destinationList;

        QDomNodeList partialRouteList = routeList.at(nodeCounter).firstChildElement("itdPartialRouteList").elementsByTagName("itdPoint");
        QDomNodeList meansOfTransportList = routeList.at(nodeCounter).firstChildElement("itdPartialRouteList").elementsByTagName("itdMeansOfTransport");
        // itdMeansOfTransport has attributes:  "network" "tC" "productName" "destination" "symbol" "motType" "spTr" "type" "name" "shortname" "destID" "TTB" "STT" "ROP"
        qDebug() << "partialRouteList size:" << partialRouteList.size() << ", meansOfTransportList:" << meansOfTransportList.size();

        for (int i = 0; i < meansOfTransportList.size(); ++i) {
            QDomElement meansOfTransportElememt = meansOfTransportList.at(i).toElement();
            QString productName = meansOfTransportElememt.attribute("productName");
            QString destination = meansOfTransportElememt.attribute("destination");
            destinationList.append(destination);
            QString name = meansOfTransportElememt.attribute("name");
            QString nameForList = name;
            if (productName == "Fussweg") {
                nameForList = "Walk";
            }
            meansOfTransportNameList.append(nameForList);
        }

        for (int i = 0; i < partialRouteList.size(); ++i) {    //itdPoint has attributes and other node
            QDomNode paritalRouteNode = partialRouteList.at(i);
            QDomElement paritalRouteElement = paritalRouteNode.toElement();
            QString partialRouteUsage = paritalRouteElement.attribute("usage");
            /* Each partialRouteList element has the following attributes: "platformName" "nameWO" "platform" "locality" "usage" "area" "name" "placeID" "stopID" "omc" "platformName"
             *  "nameWO" "platform" "locality" "usage" "area" "name" "placeID" "stopID" "omc"
             *  The following attributes are also present on partialRoutes if the arrival time is a real time
             *  "x" "y" "mapName" "x" "y" "mapName"
            */
            QString stationName = paritalRouteElement.attribute("name");
            QString platformName = paritalRouteElement.attribute("platformName");
            if (partialRouteUsage == "departure" || partialRouteUsage == "arrival" ) {
                QDomNodeList departureInformationList = paritalRouteNode.childNodes();
                for (int k = 0; k < departureInformationList.size(); ++k) {
                    QDomElement dateTimeElement = departureInformationList.at(k).toElement();
                    if (dateTimeElement.tagName() == "itdDateTime") {
                        QDateTime dateTime = parseItdDateTime(dateTimeElement);
                        if (partialRouteUsage == "departure") {
                            departureTimesList.append(dateTime);
                        } else {
                            arrivalTimesList.append(dateTime);
                        }
                        stationNameList.append(stationName);
                        platformNameList.append(platformName);
                    }
                }
            }
        }
        departureDateTime = departureTimesList.first();
        qDebug() << "Departure time set:" << departureDateTime;
        arrivalDateTime = arrivalTimesList.last();

        // Create a combined list for the detail view, combine departure, location and means of transport to a single item
        int departureCounter = 0;
        int arrivalCounter = 0;
        int meansOfTransportCounter = 0;
        for(int counter = 0 ; counter < stationNameList.size() ; counter=counter+2) {
            int counterNext = counter+1;
            if(!departureTimesList[departureCounter].isNull() || !arrivalTimesList[arrivalCounter].isNull()){
                if(departureCounter < departureTimesList.size() && counter < platformNameList.size() && meansOfTransportCounter < meansOfTransportNameList.size() && arrivalCounter < arrivalTimesList.size()) {
                    //QString trip = departureTimesList[departureCounter] + "::" + stationNameList[counter] + "::" + platformNameList[counter] + "::" + meansOfTransportNameList[meansOfTransportCounter] + "::" + destinationList[meansOfTransportCounter] + "::" + arrivalTimesList[arrivalCounter] + "::" + stationNameList[counterNext] + "::" + platformNameList[counterNext];
                    //tripList.append(trip);

                    JourneyDetailResultItem *jdrItem = new JourneyDetailResultItem();
                    jdrItem->setDepartureStation(stationNameList[counter]);
                    jdrItem->setDepartureInfo(platformNameList[counter]);
                    jdrItem->setDepartureDateTime(departureTimesList[departureCounter]);
                    jdrItem->setArrivalStation(stationNameList[counterNext]);
                    jdrItem->setArrivalInfo(platformNameList[counterNext]);
                    jdrItem->setArrivalDateTime(arrivalTimesList[arrivalCounter]);
                    jdrItem->setTrain(meansOfTransportNameList[meansOfTransportCounter]);
                    jdrItem->setDirection(destinationList[meansOfTransportCounter]);
                    jdrItem->setInternalData1("NO setInternalData1");
                    jdrItem->setInternalData2("NO setInternalData2");

                    detailsList->appendItem(jdrItem);


                    ++meansOfTransportCounter;
                }
                else  {
                    qDebug() << "ERROR - Array index issue";
                    qDebug() << "departureCounter:" << departureCounter << departureTimesList.size();
                    qDebug() << "counter used for platform:" << counter << platformNameList.size();
                    qDebug() << "meansOfTransportCounter:" << meansOfTransportCounter << meansOfTransportNameList.size();
                    qDebug() << "arrivalCounter:" << arrivalCounter << arrivalTimesList.size();
                }
                //qDebug() << trip;
            }
            ++departureCounter;
            ++arrivalCounter;
        }

        item->setDate(departureDateTime.date());
        item->setId(QString::number(nodeCounter+1));
        item->setTransfers(QString::number(numberOfChanges));
        item->setDuration(duration);
        meansOfTransportNameList.removeDuplicates();
        item->setTrainType(meansOfTransportNameList.join(", ").trimmed());
        item->setDepartureTime(departureDateTime.toString("hh:mm"));
        item->setArrivalTime(arrivalDateTime.toString("hh:mm"));

        lastJourneyResultList->appendItem(item);

        detailsList->setId(QString::number(nodeCounter+1));
        detailsList->setDepartureStation(lastJourneyResultList->departureStation());
        detailsList->setViaStation(lastJourneyResultList->viaStation());
        detailsList->setArrivalStation(lastJourneyResultList->arrivalStation());
        detailsList->setDuration(item->duration());
        detailsList->setArrivalDateTime(arrivalDateTime);
        detailsList->setDepartureDateTime(departureDateTime);
        cachedJourneyDetailsEfa[QString::number(nodeCounter+1)] = detailsList;

        if (!m_earliestArrival.isValid() || arrivalDateTime < m_earliestArrival)
            m_earliestArrival = arrivalDateTime.addSecs(-60);
        if (!m_latestResultDeparture.isValid() || departureDateTime > m_latestResultDeparture)
            m_latestResultDeparture = departureDateTime.addSecs(60);

    }
    checkForError(&doc);

    emit journeyResult(lastJourneyResultList);
}

void ParserEFA::getJourneyDetails(const QString &id)
{
    qDebug() << "ParserEFA::getJourneyDetails";
    if (currentRequestState != FahrplanNS::noneRequest) {
        return;
    }

    qDebug() << "ParserEFA::getJourneyDetails - 1";
    emit journeyDetailsResult(cachedJourneyDetailsEfa.value(id, NULL));
    return;
}


void ParserEFA::searchJourneyLater()
{
    qDebug() << "ParserEFA::searchJourneyLater()";

    if (m_latestResultDeparture.isValid())
    {
        qDebug() << "m_latestResultDeparture.isValid()";
        searchJourney(m_searchJourneyParameters.departureStation, m_searchJourneyParameters.viaStation, m_searchJourneyParameters.arrivalStation, m_latestResultDeparture, Departure, 0);
    }
    else {
        qDebug() << "!m_latestResultDeparture.isValid(), ";
        JourneyResultList *journeyResultList = new JourneyResultList();
        journeyResultList->setDepartureStation(m_searchJourneyParameters.departureStation.name);
        journeyResultList->setViaStation(m_searchJourneyParameters.viaStation.name);
        journeyResultList->setArrivalStation(m_searchJourneyParameters.arrivalStation.name);
        //: DATE, TIME
        journeyResultList->setTimeInfo(tr("%1, %2", "DATE, TIME").arg(m_searchJourneyParameters.dateTime.date().toString(Qt::DefaultLocaleShortDate)).arg(m_searchJourneyParameters.dateTime.time().toString(Qt::DefaultLocaleShortDate)));
        emit journeyResult(journeyResultList);
    }
}

void ParserEFA::searchJourneyEarlier()
{
    qDebug() << "ParserEFA::searchJourneyEarlier()";
    if (m_earliestArrival.isValid())
        searchJourney(m_searchJourneyParameters.departureStation, m_searchJourneyParameters.viaStation, m_searchJourneyParameters.arrivalStation, m_earliestArrival, Arrival, 0);
    else {
        JourneyResultList *journeyResultList = new JourneyResultList();
        journeyResultList->setDepartureStation(m_searchJourneyParameters.departureStation.name);
        journeyResultList->setViaStation(m_searchJourneyParameters.viaStation.name);
        journeyResultList->setArrivalStation(m_searchJourneyParameters.arrivalStation.name);
        //: DATE, TIME
        journeyResultList->setTimeInfo(tr("%1, %2", "DATE, TIME").arg(m_searchJourneyParameters.dateTime.date().toString(Qt::DefaultLocaleShortDate)).arg(m_searchJourneyParameters.dateTime.time().toString(Qt::DefaultLocaleShortDate)));
        emit journeyResult(journeyResultList);
    }
}

void ParserEFA::parseTimeTable(QNetworkReply *networkReply)
{
    qDebug() << "ParserEFA::parseTimeTable(networkReply.url()=" << networkReply->url().toString() << ")";

    TimetableEntriesList result;
    QDomDocument doc("result");

    QByteArray data = readNetworkReply(networkReply);
    if (doc.setContent(data, false)) {
        QDomElement departureMonitorRequestElement = doc.firstChildElement("itdRequest").firstChildElement("itdDepartureMonitorRequest");
        QDomElement referenceDateTimeElement = departureMonitorRequestElement.firstChildElement("itdDateTime");
        const QDateTime referenceDateTime = parseItdDateTime(referenceDateTimeElement);

        QDomNodeList nodeList = doc.elementsByTagName("itdDeparture");
        if (nodeList.isEmpty()) {
            nodeList = doc.elementsByTagName("itdDateTime");
        }
        for(int i = 0; i < nodeList.size(); ++i) {
            QDomNode node = nodeList.item(i);
            TimetableEntry item;

            item.platform = getAttribute(node, "platformName");
            QDomElement servingLineElement = node.firstChildElement("itdServingLine");
            item.destinationStation = getAttribute(servingLineElement, "direction");
            item.trainType = getAttribute(servingLineElement, "motType");
            QDomElement dateTimeElement = node.firstChildElement("itdDateTime");
            const QDateTime scheduledDateTime = parseItdDateTime(dateTimeElement);
            item.time = scheduledDateTime.time();
            const QString realTimeStr = getAttribute(node, "countdown");    // In minutes from search time
            if (!realTimeStr.isEmpty()) {
                const int realCountdown = realTimeStr.toInt();
                const int scheduledCountdown = qRound(referenceDateTime.secsTo(scheduledDateTime) / 60.);
                const int minutesTo = realCountdown - scheduledCountdown;
                if (minutesTo > 3) {
                    //qDebug() << "Running late";
                    item.miscInfo = tr("<span style=\"color:#b30;\">%1 min late</span>").arg(minutesTo);
                }
                else                {
                    //qDebug() << "Running on-time";
                    item.miscInfo = tr("<span style=\"color:#093; font-weight: normal;\">on time</span>");
                }
            }

            result << item;
        }
        //Check for error: <itdMessage type="error" module="BROKER" code="-4050">no serving lines found</itdMessage>
        checkForError(&doc);
    }

    emit timetableResult(result);
}

QDateTime ParserEFA::parseItdDateTime(const QDomElement &element)
{
    QDomElement dateElement = element.firstChildElement("itdDate");
    QDomElement timeElement = element.firstChildElement("itdTime");
    const QDate date(getAttribute(dateElement, "year").toInt(), getAttribute(dateElement, "month").toInt(), getAttribute(dateElement, "day").toInt());
    const QTime time(getAttribute(timeElement, "hour").toInt(), getAttribute(timeElement, "minute").toInt(), 0);
    return QDateTime(date, time);
}

QByteArray ParserEFA::readNetworkReply(QNetworkReply *networkReply)
{
    QByteArray data = networkReply->readAll();
    QByteArray gzipHeader;
    gzipHeader.resize(2);
    gzipHeader[0] = 0x1F;
    gzipHeader[1] = 0x8B;
    if (data.startsWith(gzipHeader)) {
        data = gzipDecompress(data);
    }
    return data;
}
