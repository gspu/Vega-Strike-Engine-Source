#include "system_factory.h"

#include "star_xml.h"
#include "planet.h"
#include "gfxlib_struct.h"
#include "universe_util.h"
#include "options.h"
#include "ai/fire.h"
#include "asteroid.h"
#include "ai/aggressive.h"
#include "enhancement.h"
#include "building.h"
#include "planetary_orbit.h"
#include "unit.h"
#include "enhancement.h"
#include "building.h"
#include "asteroid.h"


// TODO: For comparison only - remove
#include "terrain.h"
#include "cont_terrain.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>

namespace pt = boost::property_tree;
namespace alg = boost::algorithm;

using std::string;
using std::map;
using std::vector;
using std::endl;


/* Discussion (Roy Falk)
 * The original xml parsing was not case sensitive
 * Also, parseFloat actually parsed to double
*/

// TODO: put this in the header
extern BLENDFUNC parse_alpha( const char* );
extern void bootstrap_draw( const string &message, Animation *SplashScreen = nullptr );
extern const vector< string >& ParseDestinations( const string &value );
extern void GetLights( const vector< GFXLight > &origlights,
                       vector< GFXLightLocal > &curlights,
                       const char *str, float lightSize );
extern float ScaleOrbitDist( bool autogenerated );
extern float ScaleJumpRadius( float radius );
extern Vector ComputeRotVel( float rotvel, const QVector &r, const QVector &s );
extern Unit * getTopLevelOwner();
extern Flightgroup * getStaticBaseFlightgroup(int faction);
extern Flightgroup * getStaticAsteroidFlightgroup(int faction);
extern void SetSubunitRotation(Unit *un, float difficulty);


// TODO: refactor this
static bool ConfigAllows( string var, float val )
{
    bool invert = false;
    if (var.length() == 0)
        return true;
    if (var[0] == '-') {
        var    = var.substr( 1 );
        invert = true;
    }
    float x = XMLSupport::parse_floatf( vs_config->getVariable( "graphics", var, "0.0" ) );
    if (var.length() == 0)
        return true;
    return invert ? -x >= val : x >= val;
}

// turn a relative path filename to <sector>/<system>
string truncateFilename(string filename) {
    string::size_type tmp;
    if ( ( tmp = filename.find( ".system" ) ) != string::npos )
        filename = filename.substr( 0, tmp );
    return filename;
}

SystemFactory::SystemFactory(string const &relative_filename, string& system_file, Star_XML *xml)
{
    root = Object();
    root.type = ("root");

    this->fullname = truncateFilename(relative_filename);

    pt::ptree tree;
    pt::read_xml(system_file, tree);
    recursiveParse(tree, root);
    recursiveProcess(xml, root, nullptr);
}



void SystemFactory::recursiveParse(pt::ptree tree, Object& object)
{
    for (const auto& iterator : tree)
    {
        Object inner_object = Object();
        inner_object.type = iterator.first;

        // Process attributes
        if(inner_object.type == "<xmlattr>")
        {
            for (const auto& attributes_iterator : iterator.second)
            {
                string key = attributes_iterator.first.data();
                string value = attributes_iterator.second.data();
                alg::to_lower(key); // to avoid various bugs, we turn all keys to lowercase
                object.attributes[key] = value;
            }

            // We've already processed the children
            continue;
        }

        // Process children
        if(!iterator.second.empty())
            recursiveParse(iterator.second, inner_object);

        object.objects.push_back(inner_object);
    }
}

void SystemFactory::recursiveProcess(Star_XML *xml, Object& object, Planet* owner, int level)
{
    xml->unitlevel = level;

    if(boost::iequals(object.type, "light"))
    {
        processLight(object);
        return; // We don't bother processing children of light, as it's done in processLight
    }
    else if(boost::iequals(object.type, "system")) processSystem(xml, object);
    else if(boost::iequals(object.type, "ring")) processRing(xml, object, owner);
    else if(boost::iequals(object.type, "SpaceElevator")) processSpaceElevator(object, owner);
    else if(boost::iequals(object.type, "planet") || boost::iequals(object.type, "jump"))
            owner = processPlanet(xml, object, owner);
    else if(boost::iequals(object.type, "fog")) processFog(xml, object, owner);
    else if(boost::iequals(object.type, "unit") || boost::iequals(object.type, "asteroid") ||
            boost::iequals(object.type, "enhancement") || boost::iequals(object.type, "vehicle") ||
            boost::iequals(object.type, "building"))
        processEnhancement(object.type, xml, object, owner);

    // Now we process children
    for (auto& child_object : object.objects)
    {
        recursiveProcess(xml, child_object, owner, level+1);
    }
}

void SystemFactory::processLight(Object& object)
{
    Light light;
    for (const auto& child_object : object.objects)
    {
        GFXColor color = initializeColor(child_object);

        if(child_object.type == "diffuse") light.diffuse = color;
        if(child_object.type == "specular") light.specular = color;
        if(child_object.type == "ambient") light.ambient = color;
    }

    lights.push_back(light);
}

void SystemFactory::processSystem(Star_XML *xml, Object& object)
{
    xml->name = getStringAttribute(object, "name");
    xml->backgroundname = getStringAttribute(object, "background");
    xml->scale *= getFloatAttribute(object, "ScaleSystem"); // Size multiplier of planets, rings and some other units
    xml->reflectivity *= getFloatAttribute(object, "reflectivity");
    xml->backgroundDegamma = getBoolAttribute(object, "backgroundDegamma", xml->backgroundDegamma);

//    Object& backgroundColor = attributes["backgroundColor"];
//    xml->backgroundColor.r = std::stof(backgroundColor["r"]);
//    xml->backgroundColor.g = std::stof(backgroundColor["g"]);
//    xml->backgroundColor.b = std::stof(backgroundColor["b"]);
//    xml->backgroundColor.a = std::stof(backgroundColor["a"]);
}

void SystemFactory::processRing(Star_XML *xml, Object& object, Planet* owner)
{
    BLENDFUNC blend_source = SRCALPHA;
    BLENDFUNC blend_destination = INVSRCALPHA;
    initializeAlpha(object, blend_source, blend_destination);

    // Get the planet the ring will encircle
    Unit *unit = static_cast<Unit*>(owner);
    if (owner == nullptr) return;
    if (unit->isUnit() != PLANETPTR) return;

    QVector R(1, 0, 0 );
    QVector S( 0, 1, 0 );

    string myfile = getStringAttribute(object, "file", "planets/ring.png");
    string varname = getStringAttribute(object, "VarName", "");

    float varvalue = getFloatAttribute(object, "VarValue", 0.0f);
    float inner_radius = getFloatAttribute(object, "InnerRadius", unit->rSize(), xml->scale, 1.25f);
    float outer_radius = getFloatAttribute(object, "OuterRadius", unit->rSize(), xml->scale, 1.75f);

    int   wrapx = getIntAttribute(object, "WrapX", 1);
    int   wrapy = getIntAttribute(object, "WrapY", 1);
    int   numslices = getIntAttribute(object, "NumSlices", 8);

    initializeQVector(object, "r", R);
    initializeQVector(object, "s", S);

    if ( ConfigAllows( varname, varvalue ) )
        owner->AddRing( myfile, inner_radius, outer_radius, R, S, numslices,
                         wrapx, wrapy, blend_source, blend_destination );
}

Planet* SystemFactory::processPlanet(Star_XML *xml, Object& object, Planet* owner)
{
    QVector S( 0, 1, 0 );
    QVector R( 0, 0, 1 );

    double const scales_product = static_cast<double>(xml->scale * ScaleOrbitDist( xml->fade ));
    initializeQVector(object, "r", R, scales_product);
    initializeQVector(object, "s", S, scales_product);

    string filename = getStringAttribute(object, "file");
    string citylights = getStringAttribute(object, "citylights");
    string technique = getStringAttribute(object, "technique");
    string unitname = getStringAttribute(object, "unit");
    string fullname = getStringAttribute(object, "name", "unknw");

    BLENDFUNC blend_source = ONE;
    BLENDFUNC blend_destination = ZERO;
    initializeAlpha(object, blend_source, blend_destination);

    map< string, string > paramOverrides;
    vector< string > destination;

    float gravity = getFloatAttribute(object, "gravity", 0.0f);
    float velocity = getFloatAttribute(object, "year", 0.0f);
    float radius = getFloatAttribute(object, "radius", 0.0f);
    float position = getFloatAttribute(object, "position", 0.0f);
    float rotational_velocity = getFloatAttribute(object, "day", 0.0f);
    int faction = 0;
    bool isDestination = false;
    bool insideout = getBoolAttribute(object, "InsideOut", false);

    vector< GFXLightLocal >curlights;

    // The following code snippet makes the planet Atlantis not glow
    // when starting the game. This is actually light reflection code.
    // If you move around, the other side is dark.
    static GFXColor planet_mat_ambient   = vs_config->getColor( "default", "planet_mat_ambient",
                                           GFXColor( 1.0,1.0,1.0,1.0 ) );
    static GFXColor planet_mat_diffuse   = vs_config->getColor( "default", "planet_mat_diffuse",
                                           GFXColor( 1.0,1.0,1.0,1.0 ) );
    static GFXColor planet_mat_specular  = vs_config->getColor( "default", "planet_mat_specular",
                                           GFXColor( 0.0,0.0,0.0,1.0 ) );
    static GFXColor planet_mat_emissive = vs_config->getColor( "default", "planet_mat_emmissive",
                                           GFXColor( 0.0,0.0,0.0,1.0 ) );
    GFXMaterial ourmat;
    GFXGetMaterial( 0, ourmat );
    setMaterialAmbient(ourmat, planet_mat_ambient);
    setMaterialDiffuse(ourmat, planet_mat_diffuse);
    setMaterialSpecular(ourmat, planet_mat_specular);
    setMaterialEmissive(ourmat, planet_mat_emissive);

    initializeMaterial(object, ourmat);
    ourmat.power = getFloatAttribute(object, "SPower", ourmat.power);

    // End planet not glow code

    // "Invisible" object
    if(object.attributes.count("reflectnolight"))
    {
        ourmat.sr = ourmat.sg = ourmat.sb = ourmat.dr = ourmat.dg =
                ourmat.db = ourmat.ar = ourmat.ag = ourmat.ab = 0;
    }

    bootstrap_draw( "Loading "+fullname );

    // Parse destinations (jump only?)
    if(object.attributes.count("destination") )
    {
        destination = ParseDestinations( object.attributes["destination"] );
        isDestination = true;
    }

    // Parse lights - for Suns
    // Discussion - the original code supported multiple lights
    // This is why we have curlights as a vector
    // If you disable this, the ship in dock will be dark. Weird.
    if(object.attributes.count("light") )
    {
        unsigned long index = 0;
        char local = 0;
        std::istringstream stream(object.attributes["light"]);
        stream >> index >> local;

        Light light = lights[index];
        GFXLight gfx_light(true, Vector(0, 0, 0));
        gfx_light.SetProperties(AMBIENT, light.ambient);
        gfx_light.SetProperties(DIFFUSE, light.diffuse);
        gfx_light.SetProperties(SPECULAR, light.specular);

        GFXLightLocal local_light;
        local_light.islocal = (local == 'l');
        local_light.ligh = gfx_light;
        curlights.push_back(local_light);
    }

    // Parse faction
    if(object.attributes.count("faction") )
    {
        int originalowner =
            FactionUtil::GetFactionIndex( UniverseUtil::GetGalaxyProperty( this->fullname, "faction" ) );
        faction = FactionUtil::GetFactionIndex( object.attributes["faction"] );
        if (faction == originalowner) {
            int ownerfaction = FactionUtil::GetFactionIndex( UniverseUtil::GetGalaxyFaction( this->fullname ) );
            faction = ownerfaction;
        }
    }

    // Adjust speed and rotation
    // Discussion - the original value (day/year) needs to be adjusted to velocity
    // by multiplying
    float float_pi = static_cast<float>(M_PI);
    float float_year_scale = static_cast<float>(game_options.YearScale);
    // TODO: turn floating point comparisons into a function
    if(std::fabs(rotational_velocity) > .00001f)
        rotational_velocity = 2.0f * float_pi / (float_year_scale * rotational_velocity);

    if(std::fabs(velocity) > .00001f)
        velocity = 2.0f * float_pi / ( float_year_scale * velocity);

    float float_scales_product = static_cast<float>(scales_product);
    xml->cursun.i = getFloatAttribute(object, "x", xml->cursun.i, float_scales_product);
    xml->cursun.j = getFloatAttribute(object, "y", xml->cursun.j, float_scales_product);
    xml->cursun.k = getFloatAttribute(object, "z", xml->cursun.k, float_scales_product);

    if(object.attributes.count("override"))
    {
        string value = object.attributes["override"];
        string::size_type eqpos = value.find_first_of('=');
        if (eqpos != string::npos)
        {
            string name = value.substr(0,eqpos);
            string override_value = value.substr(eqpos+1);
            paramOverrides[name] = override_value;
        }
    }


    //xml->fade saves if it's autogenerated system
    // TODO: refactor this much later. Code smells.
    if (isDestination == true && xml->fade)
        radius = ScaleJumpRadius( radius );

    radius *= xml->scale;

    // Create Planet
    // A lot of the issues with the old code were due to the fact each XML element was parsed separately
    // with no awareness of other elements before. Therefore, state needed to be saved during processing.
    // Here we see this with the planet "owner" - I assume it is what the planet is orbiting.

    // Top level objects orbit the center?
    QVector orbit_center = QVector(0,0,0);
    if(owner == nullptr) {
        // For top level, we introduce this instead
        orbit_center = xml->cursun.Cast()+xml->systemcentroid.Cast();
    }

    Vector computed_rotational_velocity = ComputeRotVel( rotational_velocity, R, S );
    Planet *planet = new GamePlanet(R, S, velocity,
                                               computed_rotational_velocity,
                                               position, gravity, radius,
                                               filename, technique, unitname,
                                               blend_source, blend_destination, destination,
                                               orbit_center, owner, ourmat, curlights, faction,
                                               fullname, insideout );

    if(owner == nullptr)
    {
        // The sun or another body orbiting the center of the system
        // Does it really orbit or is it stationary?
        planet->SetPosAndCumPos(R+S+xml->cursun.Cast()+xml->systemcentroid.Cast());
        planet->SetOwner(getTopLevelOwner());
        xml->moons.push_back(planet); // We store top level in moons and AddUnit them in
        // star_system_xml
    } else {
        // It's a planet or moon or some other satellite
        owner->satellites.prepend(planet);
        planet->SetOwner(owner);
        owner->SetOwner(getTopLevelOwner());
    }

    planet->applyTechniqueOverrides(paramOverrides);
    return planet;
}

void SystemFactory::processSpaceElevator(Object& object, Planet* owner)
{
    string myfile = getStringAttribute(object, "file", "elevator");
    string varname = getStringAttribute(object, "varname");
    float  varvalue = getFloatAttribute(object, "varvalue", 0.0f);

    Unit *unit = static_cast<Unit*>(owner);

    if (owner == nullptr || unit->isUnit() != PLANETPTR) return;


    char direction = getCharAttribute(object, "direction", 'b');

    QVector R( 1, 0, 0 );
    QVector S( 0, 1, 0 );

    // Faction
    string faction(UniverseUtil::GetGalaxyFaction(fullname));
    if(object.attributes.count("faction"))
    {
        faction = object.attributes["faction"];
        if (faction == UniverseUtil::GetGalaxyProperty(fullname, "faction"))
        {
            string ownerfaction = UniverseUtil::GetGalaxyFaction(fullname);
            faction = ownerfaction;
        }
    }

    if(ConfigAllows( varname, varvalue ) )
        owner->AddSpaceElevator(myfile, faction, direction);
}


void SystemFactory::processFog(Star_XML *xml, Object& object, Planet* owner)
{
    if (!game_options.usePlanetFog)
        return;

    // TODO: we no longer need to use xml->fog for this
    xml->fogopticalillusion = getBoolAttribute(object, "fog", true);
    xml->fog.clear();

    for (const auto& child_object : object.objects)
    {
        AtmosphericFogMesh fogMesh = AtmosphericFogMesh();
        fogMesh.meshname = getStringAttribute(object, "file");
        fogMesh.scale = 1.1-.075+.075* (xml->fog.size()+1);

        initializeColor(child_object);

        fogMesh.er = getFloatAttribute(object, "red", fogMesh.er);
        fogMesh.eg = getFloatAttribute(object, "green", fogMesh.eg);
        fogMesh.eb = getFloatAttribute(object, "blue", fogMesh.eb);
        fogMesh.ea = getFloatAttribute(object, "alfa", fogMesh.ea);
        fogMesh.ea = getFloatAttribute(object, "alpha", fogMesh.ea);

        fogMesh.dr = getFloatAttribute(object, "dred", fogMesh.dr);
        fogMesh.dg = getFloatAttribute(object, "dgreen", fogMesh.dg);
        fogMesh.db = getFloatAttribute(object, "dblue", fogMesh.db);
        fogMesh.da = getFloatAttribute(object, "dalfa", fogMesh.da);
        fogMesh.da = getFloatAttribute(object, "dalpha", fogMesh.da);

        fogMesh.min_alpha = static_cast<int>(getFloatAttribute(object, "minalpha", fogMesh.min_alpha)) * 255;
        fogMesh.max_alpha = static_cast<int>(getFloatAttribute(object, "maxalpha", fogMesh.max_alpha)) * 255;
        fogMesh.concavity = getDoubleAttribute(object, "concavity");
        fogMesh.focus = getDoubleAttribute(object, "focus", fogMesh.focus);
        fogMesh.tail_mode_start = getIntAttribute(object, "TailModeStart", fogMesh.tail_mode_start);
        fogMesh.tail_mode_end = getIntAttribute(object, "TailModeEnd", fogMesh.tail_mode_end);
        fogMesh.scale = getDoubleAttribute(object, "ScaleAtmosphereHeight", fogMesh.scale);

        xml->fog.push_back( AtmosphericFogMesh() );
    }

    if (owner != nullptr)
        owner->AddFog( xml->fog, xml->fogopticalillusion );
}

void SystemFactory::processEnhancement(string element, Star_XML *xml, Object& object, Planet* owner)
{
    QVector S( 0, 1, 0 );
    QVector R( 0, 0, 1 );

    float const scales_product_float = xml->scale * ScaleOrbitDist( xml->fade );
    double const scales_product = static_cast<double>(scales_product_float);
    initializeQVector(object, "r", R, scales_product);
    initializeQVector(object, "s", S, scales_product);

    string filename = getStringAttribute(object, "file");
    string fullname = getStringAttribute(object, "name", "unkn-unit");
    string condition = getStringAttribute(object, "condition");

    string varname = getStringAttribute(object, "varname");
    // float  varvalue = getFloatAttribute(object, "varvalue", 0.0f);

    vector< string > destinations;

    int faction = 0;
    int neutralfaction = FactionUtil::GetNeutralFaction();

    float scalex = getFloatAttribute(object, "difficulty",
                                     static_cast<float>(game_options.AsteroidDifficulty));
    float absolute_scalex = std::fabs(scalex);
    double velocity = getDoubleAttribute(object, "year", 0.0);
    float rotational_velocity = getFloatAttribute(object, "day", 0.0f);
    double position = getDoubleAttribute(object, "position", 0.0);

    // Parse destinations (jump only?)
    if(object.attributes.count("destination") )
    {
        destinations = ParseDestinations( object.attributes["destination"] );
    }

    // Parse faction
    // This code is nonsensical to some degree.
    // Why do we need ownerfaction and not just assign to faction?
    if(object.attributes.count("faction") )
    {
        int originalowner = FactionUtil::GetFactionIndex(
                    UniverseUtil::GetGalaxyProperty( this->fullname, "faction" ) );
        faction = FactionUtil::GetFactionIndex( object.attributes["faction"] );
        if (faction == originalowner) {
            int ownerfaction = FactionUtil::GetFactionIndex( UniverseUtil::GetGalaxyFaction( this->fullname ) );
            faction = ownerfaction;
        }
    }

    xml->cursun.i = getFloatAttribute(object, "x", xml->cursun.i, scales_product_float);
    xml->cursun.j = getFloatAttribute(object, "y", xml->cursun.j, scales_product_float);
    xml->cursun.k = getFloatAttribute(object, "z", xml->cursun.k, scales_product_float);

    // Adjust speed and rotation
    // Discussion - the original value (day/year) needs to be adjusted to velocity
    // by multiplying
    // Comparing floating point with == or != is unsafe. Therefore > 0.0001
    // I assume negative means counter movement and therefore fabs
    // TODO: this code is repeated. Refactor into function
    float float_pi = static_cast<float>(M_PI);
    float float_year_scale = static_cast<float>(game_options.YearScale);
    // TODO: turn floating point comparisons into a function
    if(std::fabs(rotational_velocity) > .00001f)
        rotational_velocity = 2.0f * float_pi / ( float_year_scale * rotational_velocity);

    if(std::fabs(velocity) > .00001)
        velocity = 2.0 * M_PI / (game_options.YearScale * velocity);


    if(boost::iequals(element, "nebula"))
        return; // Nebula not supported at present


    // This condition is probably never met
//    if ( ( !xml->conditionStack.size() || xml->conditionStack.back() )
//        && ConfigAllows( varname, varvalue )
//        && ConfigCondition( condition ) ) return;

    // Discussion - this code was previously something of a mess.
    // I've refactored it to reduce complexity and improve readability
    Unit* unit = nullptr;

    if(boost::iequals(element, "unit")) {
        Flightgroup *fg = getStaticBaseFlightgroup(faction);
        unit = new GameUnit< Unit >( filename.c_str(), false, faction, "", fg, fg->nr_ships-1 );
        unit->setFullname(fullname);

        if(unit->faction != neutralfaction) {
            unit->SetTurretAI(); //FIXME un de-referenced before allocation
            unit->EnqueueAI( new Orders::FireAt( 15 ) ); //FIXME un de-referenced before allocation
        }
    } else if(boost::iequals(element, "asteroid")) {
        Flightgroup *fg = getStaticAsteroidFlightgroup(faction);
        unit = static_cast<Unit*>(
                    new Asteroid( filename.c_str(),
                                                 faction, fg, fg->nr_ships-1,
                                                 absolute_scalex ));
        if (scalex < 0) // This was almost certainly fixed by the above line. TODO: refactor
            SetSubunitRotation(unit, absolute_scalex); //FIXME un de-referenced before allocation

    } else if(boost::iequals(element, "enhancement")) {
        unit = static_cast<Unit*>(
                    new GameEnhancement(filename.c_str(), faction, string("")));

    } else if(boost::iequals(element, "building") ||
              boost::iequals(element, "vehicle")) {

        if (xml->ct == nullptr && xml->parentterrain != nullptr) // Terrain
            unit = static_cast<Unit*>(
                        new GameBuilding(xml->parentterrain,
                                                    boost::iequals(element, "vehicle"),
                                                    filename.c_str(), false, faction,
                                                    string("")));
        else if(xml->ct != nullptr) // Continuous terrain
            unit = static_cast<Unit*>(
                        new GameBuilding(xml->ct,
                                                    boost::iequals(element, "vehicle"),
                                                    filename.c_str(), false, faction,
                                                    string("")));

        unit->EnqueueAI(new Orders::AggressiveAI( "default.agg.xml"));
        unit->SetTurretAI(); // This was only applied to ct in original code
    }

    for (auto& destination : destinations)
        unit->AddDestination(destination);

    if(owner == nullptr)
    {
        // Top level element. e.g. a sun
        unit->SetAI( new PlanetaryOrbit(unit, velocity, position, R, S, xml->cursun.Cast()+xml->systemcentroid.Cast(), nullptr));

        unit->SetPosAndCumPos( R+S+xml->cursun.Cast()+xml->systemcentroid.Cast() );
        unit->SetOwner( getTopLevelOwner() );
        xml->moons.push_back(static_cast<Planet*>(unit)); // Calling factory will call AddUnit using this
    } else {
        // Some kind of satellite.
        owner->AddSatellite(unit);
        unit->SetOwner(owner);
        //cheating so nothing collides at top level - is this comment still relevant?
        // FIXME un de-referenced before allocation - is this comment still relevant?
        unit->SetAngularVelocity(ComputeRotVel(rotational_velocity, R, S));
        unit->SetAI(new PlanetaryOrbit(unit, velocity, position, R, S, QVector(0, 0, 0), owner));
    }



}



// Discussion (Roy Falk): I initially thought to make this a template. Then you would automatically
// get the value required. i.e. T t = getAttribute(...) would return T.
// However, we would still need to specialize the string conversion (e.g. std:stof)
// More important, this is dangerous. Changing T would change the function called and
// this would not be obvious to somewhat not familiar with the code.
string SystemFactory::getStringAttribute(Object object, string key, string default_value)
{
    alg::to_lower(key);
    if(object.attributes.count(key)) return object.attributes[key];
    return default_value;
}

bool SystemFactory::getBoolAttribute(Object object, string key, bool default_value)
{
    alg::to_lower(key);
    if(object.attributes.count(key)) return object.attributes[key] == "true";
    return default_value;
}

char SystemFactory::getCharAttribute(Object object, string key, char default_value)
{
    alg::to_lower(key);
    if(object.attributes.count(key) && object.attributes[key].size()>0) return object.attributes[key][0];
    return default_value;
}

int SystemFactory::getIntAttribute(Object object, string key, int default_value,
                                int multiplier, int default_multiplier)
{
    alg::to_lower(key);
    if(object.attributes.count(key)) return std::stoi(object.attributes[key]) * multiplier;
    return default_value * default_multiplier;
}

float SystemFactory::getFloatAttribute(Object object, string key, float default_value,
                                float multiplier, float default_multiplier)
{
    alg::to_lower(key);
    if(object.attributes.count(key)) return std::stof(object.attributes[key]) * multiplier;
    return default_value * default_multiplier;
}

double SystemFactory::getDoubleAttribute(Object object, string key, double default_value,
                                double multiplier, double default_multiplier)
{
    alg::to_lower(key);
    if(object.attributes.count(key)) return std::stod(object.attributes[key]) * multiplier;
    return default_value * default_multiplier;
}

void SystemFactory::initializeQVector(Object object, string key_prefix, QVector& vector, double multiplier)
{
    vector.i = getDoubleAttribute(object, key_prefix + "i", vector.i, multiplier);
    vector.j = getDoubleAttribute(object, key_prefix + "j", vector.j, multiplier);
    vector.k = getDoubleAttribute(object, key_prefix + "k", vector.k, multiplier);
}

void SystemFactory::initializeMaterial(Object object, GFXMaterial& material)
{
    material.er = getFloatAttribute(object, "Red", material.er);
    material.eg = getFloatAttribute(object, "Green", material.eg);
    material.eb = getFloatAttribute(object, "Blue", material.eb);
    material.ea = getFloatAttribute(object, "Alfa", material.ea);
    material.dr = getFloatAttribute(object, "DRed", material.dr);
    material.dg = getFloatAttribute(object, "DGreen", material.dg);
    material.db = getFloatAttribute(object, "DBlue", material.db);
    material.da = getFloatAttribute(object, "DAlfa", material.da);
    material.sr = getFloatAttribute(object, "SRed", material.sr);
    material.sg = getFloatAttribute(object, "SGreen", material.sg);
    material.sb = getFloatAttribute(object, "SBlue", material.sb);
    material.sa = getFloatAttribute(object, "SAlfa", material.sa);
}

// Parse Alpha
// TODO: refactor
// Discussion of blendSrc and blendDst (Roy Falk)
// If we don't have alpha, we use SRCALPHA/INVSRCALPHA
// If we do but it's invalid, we use ONE/ZERO
// Otherwise we actually parse it
// This doesn't seem right
void SystemFactory::initializeAlpha(Object object, BLENDFUNC blend_source, BLENDFUNC blend_destination)
{
    if(!object.attributes.count("alpha")) return;

    blend_source = ONE;
    blend_destination = ZERO;


    const char *alpha = object.attributes["alpha"].c_str();
    if(alpha == nullptr || alpha[0] == 0) return; // This is really only a check for an empty string. Refactor?

    char *s = strdup( alpha );
    char *d = strdup( alpha );

    blend_source = SRCALPHA;
    blend_destination = INVSRCALPHA;

    if ( 2 == sscanf( alpha, "%s %s", s, d ) ) {
        if (strcmp( s, "true" ) != 0) {
            blend_source = parse_alpha( s );
            blend_destination = parse_alpha( d );
        }
    }

    free( s );
    free( d );
}

GFXColor SystemFactory::initializeColor(Object object)
{
    return GFXColor(getFloatAttribute(object, "red", 0),
                    getFloatAttribute(object, "green", 0),
                    getFloatAttribute(object, "blue", 0),
                    getFloatAttribute(object, "alpha", 1));
}
