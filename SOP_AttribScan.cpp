#include "SOP_AttribScan.h"
#include "SOP_AttribScan.proto.h"

#include <GA/GA_Primitive.h>
#include <GU/GU_Detail.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_TemplateBuilder.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_StringHolder.h>
#include <SYS/SYS_Math.h>
#include <limits.h>
#include <sstream>

struct scan_parameters {
	size_t attrib_type;
	size_t attrib_owner;
	std::variant<
		float, 
		int, 
		std::string,
		UT_Vector2D,
		UT_Vector3D,
		UT_Vector4D
	> val;
	bool ensure;
	size_t validation_method;
};

enum Attrib_Class {
	GA_ATTRIB_FLOAT,
	GA_ATTRIB_INT,
	GA_ATTRIB_STRING,
	GA_ATTRIB_VECTOR2,
	GA_ATTRIB_VECTOR3,
	GA_ATTRIB_VECTOR4,
	GA_ATTRIB_MATRIX2,
	GA_ATTRIB_MATRIX3,
	GA_ATTRIB_MATRIX4,
	GA_ATTRIB_DICT
};

typedef void (*ValidatePtr) (GA_Attribute*, const GU_Detail*);

const UT_StringHolder SOP_AttribScan::theSOPTypeName("bears_attrib_scan"_sh);

void newSopOperator(OP_OperatorTable* table) {
	table->addOperator(new OP_Operator(
		SOP_AttribScan::theSOPTypeName,
		"Attribute Scan",
		SOP_AttribScan::myConstructor,
		SOP_AttribScan::buildTemplates(),
		1,
		1,
		nullptr,
		0
	));
}

// Step 2. Write this up so we can use %HHP%\generate_proto.py to create our
//         proto header to finish the rest of the implementation.
static const char* theDsFile = R"THEDSFILE(
{
	name		parameters
	groupsimple {
		name "attributes_folder"
		label "Attributes"

		parm {
			name "ptscan"
			label "Point Attributes"
			type string
			default { "" }
		}
		parm {
			name "primscan"
			label "Primitive Attributes"
			type string
			default { "" }
		}
		parm {
			name "vtxscan"
			label "Vertex Attributes"
			type string
			default { "" }
		}
		parm {
			name "dtlscan"
			label "Detail Attributes"
			type string
			default { "" }
		}
	}
	multiparm {
		name "recognizedattribs"
		cppname "RecognizedAttribs"
		label "Recognized Attribs"
		default 1

		parm {
			name "attribname#"
			label "Name"
			type string
			joinnext
		}
		parm {
			name "attribowner#"
			label "Scope"
			type ordinal
			joinnext
			menu {
				"verts" "Vertices"
				"points" "Points"
				"prims" "Primitives"
				"detail" "Detail"
			}
		}
		parm {
			name "attribtype#"
			label "Type"
			type ordinal
			menu {
				"float" "Float"
				"int" "Int"
				"string" "String"
				"vector2" "Vector2"
				"vector" "Vector3"
				"vector4" "Vector4"
				"matrix2" "Matrix2"
				"matrix3" "Matrix3"
				"matrix" "Matrix4"
				"dict" "Dict"				

			}
		}
		parm {
			name "ensurevalue#"
			label "Ensure Value"
			type toggle
			joinnext
			hidewhen "{ attribtype# == matrix2 } { attribtype# == matrix3 } { attribtype# == matrix } { attribtype# == dict }"
		}
		parm {
			name "floatval#"
			label "Value"
			type float
			joinnext
			disablewhen "{ ensurevalue# == 0 }"
			hidewhen "{ attribtype# != float }"
		}
		parm {
			name "intval#"
			label "Value"
			type integer
			joinnext
			disablewhen "{ ensurevalue# == 0 }"
			hidewhen "{ attribtype# != int }"
		}
		parm {
			name "stringval#"
			label "Value"
			type string
			joinnext
			disablewhen "{ ensurevalue# == 0 }"
			hidewhen "{ attribtype# != string }"
		}
		parm {
			name "vector2val#"
			label "Value"
			type float
			size 2
			joinnext
			disablewhen "{ ensurevalue# == 0 }"
			hidewhen "{ attribtype# != vector2 }"
		}
		parm {
			name "vector3val#"
			label "Value"
			type float
			size 3
			joinnext
			disablewhen "{ ensurevalue# == 0 }"
			hidewhen "{ attribtype# != vector }"
		}
		parm {
			name "vector4val#"
			label "Value"
			type float
			size 4
			joinnext
			disablewhen "{ ensurevalue# == 0 }"
			hidewhen "{ attribtype# != vector4 }"
		}
		parm {
			name "method#"
			label "Method"
			type ordinal
			nolabel
			disablewhen "{ ensurevalue# == 0 }"
			hidewhen "{ attribtype# == matrix2 } { attribtype# == matrix3 } { attribtype# == matrix } { attribtype# == dict }"
			menu {
				"presence" "Presence"
				"absence" "Absence"
			}
		}
	}
}
)THEDSFILE";

PRM_Template* SOP_AttribScan::buildTemplates()
{
	static PRM_TemplateBuilder templ("SOP_AttribScan.cpp"_sh, theDsFile);
	return templ.templates();
}

class SOP_AttribScanVerb : public SOP_NodeVerb
{
public:
	SOP_AttribScanVerb() {}
	virtual ~SOP_AttribScanVerb() {}

	virtual SOP_NodeParms* allocParms() const { return new SOP_AttribScanParms(); }
	virtual UT_StringHolder name() const { return SOP_AttribScan::theSOPTypeName; }

	virtual CookMode cookMode(const SOP_NodeParms* parms) const { return COOK_INPLACE; }

	virtual void cook(const CookParms& cookparms) const;

	static const SOP_NodeVerb::Register<SOP_AttribScanVerb> theVerb;
};

const SOP_NodeVerb::Register<SOP_AttribScanVerb> SOP_AttribScanVerb::theVerb;
const SOP_NodeVerb* SOP_AttribScan::cookVerb() const {
	return SOP_AttribScanVerb::theVerb.get();
}

template<typename... Args>
std::string format_string(const std::string& target, Args...args) {
	int size_s = std::snprintf(nullptr, 0, target.c_str(), args...) + 1;
	size_t size = static_cast<size_t>(size_s);
	auto buf = std::make_unique<char[]>(size);
	std::snprintf(buf.get(), size, target.c_str(), args...);
	return std::string(buf.get(), buf.get() + size - 1);
}

std::string missing_attrib_msg(const std::string& attrib_name, const char* owner) {
	std::string msg{ "\nMissing %s attribute %s" };
	return format_string(msg, owner, attrib_name);
}

std::string unexpected_type_msg(const std::string& attrib_name, const size_t& index, const char* owner) {
	std::vector<std::string> type_map{
		"float", "int", "string", 
		"vector2", "vector3", "vector4", 
		"matrix2", "matrix3", "matrix", 
		"dict"
	};
	std::string type{ type_map[index] };
	std::string msg{ "\n%s attribute %s is unexpected type, expecting type %s" };
	return format_string(msg, owner, attrib_name, type);
}

std::string irregular_values_msg(const std::string& attrib_name, std::string& owner) {
	std::string msg{ "\nIrregular values on %s attribute %s" };
	return format_string(msg, owner, attrib_name);
}

std::string undesired_values_msg(const std::string& attrib_name, std::string& owner) {
	std::string msg{ "\nUndesired values on %s attribute %s" };
	return format_string(msg, owner, attrib_name);
}

bool validate_float(GA_Attribute* attrib) {
	return static_cast<bool>(attrib->getStorageClass() == GA_STORECLASS_FLOAT && attrib->getTupleSize() == 1);
}

bool validate_int(GA_Attribute* attrib) {
	return static_cast<bool>(attrib->getStorageClass() == GA_STORECLASS_INT && attrib->getTupleSize() == 1);
}

bool validate_string(GA_Attribute* attrib) {
	return static_cast<bool>(attrib->getStorageClass() == GA_STORECLASS_STRING);
}

bool validate_dict(GA_Attribute* attrib) {
	return static_cast<bool>(attrib->getStorageClass() == GA_STORECLASS_DICT);
}

bool validate_vector2(GA_Attribute* attrib) {
	return static_cast<bool>(attrib->getStorageClass() == GA_STORECLASS_FLOAT && attrib->getTupleSize() == 2);
}

bool validate_vector3(GA_Attribute* attrib) {
	return static_cast<bool>(attrib->getStorageClass() == GA_STORECLASS_FLOAT && attrib->getTupleSize() == 3);
}

bool validate_vector4(GA_Attribute* attrib) {
	return static_cast<bool>(attrib->getStorageClass() == GA_STORECLASS_FLOAT && attrib->getTupleSize() == 4);
}

bool validate_matrix2(GA_Attribute* attrib) {
	return static_cast<bool>(attrib->getStorageClass() == GA_STORECLASS_FLOAT && attrib->getTupleSize() == 4);
}

bool validate_matrix3(GA_Attribute* attrib) {
	return static_cast<bool>(attrib->getStorageClass() == GA_STORECLASS_FLOAT && attrib->getTupleSize() == 9);
}

bool validate_matrix4(GA_Attribute* attrib) {
	return static_cast<bool>(attrib->getStorageClass() == GA_STORECLASS_FLOAT && attrib->getTupleSize() == 16);
}

GA_Range get_range(const GA_Detail& detail, size_t attrib_owner) {
	switch (attrib_owner) {
		case GA_ATTRIB_VERTEX:
			return detail.getVertexRange();
		case GA_ATTRIB_POINT:
			return detail.getPointRange();
		case GA_ATTRIB_PRIMITIVE:
			return detail.getPrimitiveRange();
		case GA_ATTRIB_DETAIL:
			return detail.getGlobalRange();
		default:
			std::cout << "You shouldn't be seeing this" << std::endl;
			return detail.getPointRange();
	}
}

template <typename T1, typename T2, typename T3>
void validate_value(
	GA_Attribute* attrib,
	const T1 val,
	size_t attrib_owner,
	size_t method_toggle,
	std::string& msg)
{
	GA_Range my_range{ get_range(attrib->getDetail(), attrib_owner) };
	std::vector<std::string> owner_map{ "vertex", "point", "primitive", "detail" };

	T2 handle{ attrib };
	if (!handle.isValid()) {
		return;
	}
	std::string attrib_name{ attrib->getName().c_str() };
	for (GA_Iterator it(my_range); !it.atEnd(); ++it) {
		GA_Offset offset = *it;
		const auto my_A{ (T3)handle.get(offset) };
		if (my_A != *val && !method_toggle) { 
			msg.append(irregular_values_msg(attrib_name, owner_map[attrib_owner]));
			return;
		}
		else if (my_A == *val && method_toggle) {
			msg.append(undesired_values_msg(attrib_name, owner_map[attrib_owner]));
			return;
		}
	}
}

void validate_string_value(
	GA_Attribute* attrib,
	const std::string* val,
	size_t attrib_owner,
	size_t method_toggle,
	std::string& msg)
{
	GA_Range my_range{ get_range(attrib->getDetail(), attrib_owner) };
	std::vector<std::string> owner_map{ "vertex", "point", "primitive", "detail" };
	GA_ROHandleS A_h{ attrib };
	if (!A_h.isValid()) {
		std::cout << "String handle not valid??" << std::endl;
		return;
	}

	std::string attrib_name{ attrib->getName().c_str() };
	for (GA_Iterator it(my_range); !it.atEnd(); ++it) {
		GA_Offset offset = *it;
		std::string my_A{ A_h.get(offset) };
		if (my_A.find(*val) && !method_toggle) {
			msg.append(irregular_values_msg(attrib_name, owner_map[attrib_owner]));
			return;
		}
		else if (!my_A.find(*val) && method_toggle) {
			msg.append(undesired_values_msg(attrib_name, owner_map[attrib_owner]));
			return;
		}
	}
}

bool validate_attribute_type(
	scan_parameters& scan_parms,
	GA_Attribute* attrib) 
{
	size_t index{ scan_parms.attrib_type };
	std::vector<bool (*) (GA_Attribute*)> scan_funcs{
		validate_float,
		validate_int,
		validate_string,
		validate_vector2,
		validate_vector3,
		validate_vector4,
		validate_matrix2,
		validate_matrix3,
		validate_matrix4,
		validate_dict
	};
	return scan_funcs[index](attrib);
}

void scan_func(
	const std::string& pattern, 
	const GA_AttributeDict& attribs,
	const std::map<std::string, scan_parameters>& owner_map,
	std::string& error_msg,
	const char* owner) 
{

	std::stringstream attrss(pattern);
	std::string temp;
	while (std::getline(attrss, temp, ' ')) {

		// search for attrib presence in provided attrib dict
		const UT_StringRef name_wrapper{ temp.c_str() };
		GA_Attribute* attrib{ attribs.find(GA_SCOPE_PUBLIC, name_wrapper) };
		if (!attrib) {
			error_msg.append(missing_attrib_msg(temp, owner));
			return;
		}

		// Ensure attrib exists on current class of geo (vert, point, prim, detail)
		// before continuing with rest of scan.
		if (!owner_map.count(temp)) {
			return;
		}

		// ensure attrib is of expected type
		scan_parameters scan_parms{ owner_map.at(temp) };
		if (!validate_attribute_type(scan_parms, attrib)) {
			error_msg.append(unexpected_type_msg(temp, scan_parms.attrib_type, owner));
			return;
		}
		if (!scan_parms.ensure)
			return;

		// ensure attrib has expected value
		if (const float* fval = std::get_if<float>(&scan_parms.val)) {
			validate_value<const float*, GA_ROHandleD, float>(attrib, fval, scan_parms.attrib_owner, scan_parms.validation_method, error_msg);
		}
		else if (const int* ival = std::get_if<int>(&scan_parms.val)) {
			validate_value<const int*, GA_ROHandleI, int>(attrib, ival, scan_parms.attrib_owner, scan_parms.validation_method, error_msg);
		}
		else if (const std::string* sval = std::get_if<std::string>(&scan_parms.val)) {
			// TODO: give own validation function to allow for substring detection rather than wholesale string comp
			validate_string_value(attrib, sval, scan_parms.attrib_owner, scan_parms.validation_method, error_msg);
		}
		else if (const UT_Vector2D* vval = std::get_if<UT_Vector2D>(&scan_parms.val)) {
			validate_value<const UT_Vector2D*, GA_ROHandleV2, UT_Vector2D>(attrib, vval, scan_parms.attrib_owner, scan_parms.validation_method, error_msg);
		}
		else if (const UT_Vector3D* vval = std::get_if<UT_Vector3D>(&scan_parms.val)) {
			validate_value<const UT_Vector3D*, GA_ROHandleV3, UT_Vector3D>(attrib, vval, scan_parms.attrib_owner, scan_parms.validation_method, error_msg);
		}
		else if (const UT_Vector4D* vval = std::get_if<UT_Vector4D>(&scan_parms.val)) {
			validate_value<const UT_Vector4D*, GA_ROHandleV4, UT_Vector4D>(attrib, vval, scan_parms.attrib_owner, scan_parms.validation_method, error_msg);
		}
		else
			std::cout << "Not valid" << std::endl;
	}
}

scan_parameters build_scan_parms(const SOP_AttribScanParms::RecognizedAttribs& recognized_attribs_pattern) {
	
	scan_parameters scan_parms{};
	scan_parms.ensure = (bool)recognized_attribs_pattern.ensurevalue;
	const size_t validation_method{ (size_t)recognized_attribs_pattern.method };
	scan_parms.validation_method = validation_method;
	const size_t attrib_owner{ (size_t)recognized_attribs_pattern.attribowner };
	scan_parms.attrib_owner = attrib_owner;
	const size_t attrib_type{ (size_t)recognized_attribs_pattern.attribtype };
	scan_parms.attrib_type = attrib_type;

	switch (attrib_type) {
		case GA_ATTRIB_FLOAT:
			scan_parms.val = (float)recognized_attribs_pattern.floatval;
			break;
		case GA_ATTRIB_INT:
			scan_parms.val = (int)recognized_attribs_pattern.intval;
			break;
		case GA_ATTRIB_STRING:
			scan_parms.val = (std::string)recognized_attribs_pattern.stringval;
			break;
		case GA_ATTRIB_VECTOR2:
			scan_parms.val = (UT_Vector2D)recognized_attribs_pattern.vector2val;
			break;
		case GA_ATTRIB_VECTOR3:
			scan_parms.val = (UT_Vector3D)recognized_attribs_pattern.vector3val;
			break;
		case GA_ATTRIB_VECTOR4:
			scan_parms.val = (UT_Vector4D)recognized_attribs_pattern.vector4val;
			break;
		default:
			scan_parms.val = -1;
			break;
	}
	return scan_parms;
}

// This is the meat and bones of the operation.
void SOP_AttribScanVerb::cook(const SOP_NodeVerb::CookParms& cookparms) const {

	const GU_Detail* detail = cookparms.gdh().gdp();
	auto&& sopparms = cookparms.parms<SOP_AttribScanParms>();

	std::map<std::string, scan_parameters> vert_map;
	std::map<std::string, scan_parameters> point_map;
	std::map<std::string, scan_parameters> prim_map;
	std::map<std::string, scan_parameters> detail_map;

	// parse multiparm
	const UT_Array<SOP_AttribScanParms::RecognizedAttribs>& recognized_attribs{ sopparms.getRecognizedAttribs() };
	for (exint attribsi = 0, n_attribs = recognized_attribs.size(); attribsi < n_attribs; ++attribsi) {
		const SOP_AttribScanParms::RecognizedAttribs& recognized_attribs_pattern{ recognized_attribs[attribsi] };
		const std::string attrib_name{ (std::string)recognized_attribs_pattern.attribname };
		if (!attrib_name.size())
			continue;

		// populate scan parameters
		scan_parameters scan_parms{ build_scan_parms(recognized_attribs_pattern) };

		// register parameters based on scope
		switch (scan_parms.attrib_owner){
			case GA_ATTRIB_VERTEX:
				vert_map.emplace((std::string)attrib_name.c_str(), scan_parms);
				break;
			case GA_ATTRIB_POINT:
				point_map.emplace((std::string)attrib_name.c_str(), scan_parms);
				break;
			case GA_ATTRIB_PRIMITIVE:
				prim_map.emplace((std::string)attrib_name.c_str(), scan_parms);
				break;
			case GA_ATTRIB_DETAIL:
				detail_map.emplace((std::string)attrib_name.c_str(), scan_parms);
				break;
			default:
				std::cout << "Please don't modify the parms" << std::endl;
		}
	}

	std::string error_msg{ "Found Following Issues:" };
	size_t base_len{ error_msg.size() };

	scan_func((std::string)sopparms.getVtxscan(), detail->vertexAttribs(), vert_map, error_msg, "vertex");
	scan_func((std::string)sopparms.getPtscan(), detail->pointAttribs(), point_map, error_msg, "point");
	scan_func((std::string)sopparms.getPrimscan(), detail->primitiveAttribs(), prim_map, error_msg, "primitive");
	scan_func((std::string)sopparms.getDtlscan(), detail->attribs(), detail_map, error_msg, "detail");

	if (error_msg.size() != base_len)
		cookparms.sopAddError(SOP_MESSAGE, error_msg.c_str());
}
