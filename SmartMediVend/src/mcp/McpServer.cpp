#include "McpServer.h"

#include "../core/JsonLite.h"

namespace smv::mcp {

std::string McpServer::handle(std::string_view request) {
  if (request.size() > kMaxRequestBytes) {
    return error("null", -32600, "REQUEST_TOO_LARGE");
  }
  if (!jsonlite::isValidObject(request)) {
    return error("null", -32700, "PARSE_ERROR");
  }

  jsonlite::ValueView version;
  jsonlite::ValueView id;
  jsonlite::ValueView method;
  if (!jsonlite::findMember(request, "jsonrpc", version) ||
      version.stringValue() != "2.0" ||
      !jsonlite::findMember(request, "id", id) ||
      !jsonlite::findMember(request, "method", method) ||
      method.kind != jsonlite::ValueKind::String) {
    return error("null", -32600, "INVALID_REQUEST");
  }
  const std::string_view requestId = id.raw;
  const std::string_view methodName = method.stringValue();

  if (methodName == "initialize") {
    return result(requestId,
                  R"({"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"SmartMediVend","version":"1.0.0"}})");
  }
  if (methodName == "tools/list") {
    return result(requestId, toolsListJson());
  }
  if (methodName != "tools/call") {
    return error(requestId, -32601, "METHOD_NOT_FOUND");
  }

  jsonlite::ValueView params;
  jsonlite::ValueView name;
  if (!jsonlite::findMember(request, "params", params) ||
      params.kind != jsonlite::ValueKind::Object ||
      !jsonlite::findMember(params.raw, "name", name) ||
      name.kind != jsonlite::ValueKind::String) {
    return error(requestId, -32602, "INVALID_PARAMS");
  }
  jsonlite::ValueView arguments;
  const std::string_view argumentJson =
      jsonlite::findMember(params.raw, "arguments", arguments)
          ? arguments.raw
          : std::string_view("{}");
  if (arguments.valid() && arguments.kind != jsonlite::ValueKind::Object) {
    return error(requestId, -32602, "INVALID_PARAMS");
  }

  auto executed = tools_.execute(name.stringValue(), argumentJson);
  if (!executed.found) {
    return error(requestId, -32601, "TOOL_NOT_FOUND");
  }
  if (!executed.ok) {
    return error(requestId, -32602, executed.errorTag);
  }
  return result(requestId, executed.json);
}

std::string McpServer::error(std::string_view id,
                             int code,
                             std::string_view message) {
  return std::string("{\"jsonrpc\":\"2.0\",\"id\":") + std::string(id) +
         ",\"error\":{\"code\":" + std::to_string(code) +
         ",\"message\":\"" + std::string(message) + "\"}}";
}

std::string McpServer::result(std::string_view id, std::string_view json) {
  return std::string("{\"jsonrpc\":\"2.0\",\"id\":") + std::string(id) +
         ",\"result\":" + std::string(json) + "}";
}

std::string McpServer::toolsListJson() {
  return R"JSON({"tools":[
    {"name":"smartmedivend.get_device_status","description":"Read-only device and safety-lock state. Never controls vending.","inputSchema":{"type":"object","additionalProperties":false}},
    {"name":"smartmedivend.get_inventory","description":"Read-only estimated blister stock. Stock is command-sent-unverified because no drop sensor is installed.","inputSchema":{"type":"object","additionalProperties":false}},
    {"name":"smartmedivend.get_medicine_info","description":"Read local pharmacist-reviewable catalog information by canonical medicine id. Never accepts SKU or channel.","inputSchema":{"type":"object","required":["canonical_id"],"properties":{"canonical_id":{"type":"string"}},"additionalProperties":false}},
    {"name":"smartmedivend.submit_symptom_data","description":"Submit only extracted patient answers. The ESP32 independently validates safety and selects at most three canonical medicines. Never send SKU, channel, relay, quantity, or vend fields.","inputSchema":{"type":"object","required":["session_id","turn_id"],"properties":{
      "session_id":{"type":"string","maxLength":128},"turn_id":{"type":"integer","minimum":0,"maximum":1000000},
      "age_years":{"type":"integer","minimum":0,"maximum":120},"weight_kg":{"type":"integer","minimum":1,"maximum":300},
      "pregnancy_or_breastfeeding":{"type":"boolean"},"duration_hours":{"type":"integer","minimum":0,"maximum":87600},
      "symptoms":{"type":"array","maxItems":8,"items":{"type":"string","enum":["fever","mild_headache","mild_body_ache","mild_inflammatory_pain","allergic_rhinitis","dry_cough","productive_cough","mild_sore_throat","gas_bloating","acid_indigestion","short_term_reflux","acute_watery_diarrhoea","short_term_constipation","motion_sickness","digestive_support","unexplained_nausea"]}},
      "danger_signs":{"type":"array","maxItems":16,"items":{"type":"string","enum":["difficulty_breathing","chest_pain","confusion","fainting","seizure","weakness_or_speech_problem","sudden_severe_headache","stiff_neck","coughing_blood","vomiting_blood","bloody_or_black_stool","severe_abdominal_pain","dehydration","cannot_keep_fluids","difficulty_swallowing","drooling","stridor","unintentional_weight_loss","rapidly_worsening"]}},
      "conditions":{"type":"array","maxItems":12,"items":{"type":"string","enum":["liver_disease","kidney_disease","heart_disease","peptic_ulcer","gastrointestinal_bleeding","nsaid_allergy","nsaid_triggered_asthma","asthma_or_copd","glaucoma","urination_difficulty","immunocompromised","critically_ill","central_venous_catheter","inflammatory_bowel_disease","bowel_obstruction","thyroid_treatment","sodium_restriction","chronic_cough","constipation","abdominal_swelling","yeast_allergy","serious_skin_or_mucosal_reaction","heavy_alcohol_use"]}},
      "current_medicines":{"type":"array","maxItems":16,"items":{"type":"string","enum":["contains_paracetamol","anticoagulant","systemic_steroid","other_nsaid","other_antihistamine","maoi_within_14_days","sedative_or_tranquilizer","levothyroxine","clopidogrel","important_ppi_interaction","recent_antacid","medicine_requiring_antacid_spacing","antifungal"]}},
      "allergies":{"type":"array","maxItems":8,"items":{"type":"string","enum":["paracetamol","ibuprofen_or_nsaid","loratadine","dextromethorphan","ambroxol","dequalinium","simethicone","aluminium_or_magnesium_antacid","omeprazole_or_ppi","loperamide","bisacodyl","dimenhydrinate","yeast"]}},
      "alcohol_use":{"type":"boolean"},"driving_or_operating_machinery":{"type":"boolean"},"diarrhoea_after_antibiotics":{"type":"boolean"},"constipation_lifestyle_tried":{"type":"boolean"}
    },"additionalProperties":false}},
    {"name":"smartmedivend.get_candidate_status","description":"Read-only status of the candidate created by the ESP32 local rule engine.","inputSchema":{"type":"object","additionalProperties":false}}
  ]})JSON";
}

}  // namespace smv::mcp
