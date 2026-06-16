// Copyright (C) 2025 ...

#include "data_products/track_start.hpp"
#include "form/form_writer.hpp"
#include "form/technology.hpp"
#include "test_helpers.hpp"
#include "test_utils.hpp"
#include "toy_tracker.hpp"

#include <TFile.h>
#include <TObjString.h>
#include <TTree.h>
#include <TUUID.h>

#include <cstdlib>
#include <ctime>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <set>
#include <string>
#include <vector>

static int const NUMBER_EVENT = 4;
static int const NUMBER_SEGMENT = 15;

void generate(std::vector<float>& vrand, int size)
{
  // NOLINTBEGIN(concurrency-mt-unsafe, cert-msc30-c, misc-predictable-rand, cert-msc50-cpp) - Single-threaded test
  int rand1 = rand() % 32768;
  int rand2 = rand() % 32768;
  // NOLINTEND(concurrency-mt-unsafe, cert-msc30-c, misc-predictable-rand, cert-msc50-cpp) - Single-threaded test
  int npx = (rand1 * 32768 + rand2) % size;
  for (int nelement = 0; nelement < npx; ++nelement) {
    // NOLINTBEGIN(concurrency-mt-unsafe, cert-msc30-c, misc-predictable-rand, cert-msc50-cpp) - Single-threaded test
    int rand1 = rand() % 32768;
    int rand2 = rand() % 32768;
    // NOLINTEND(concurrency-mt-unsafe, cert-msc30-c, misc-predictable-rand, cert-msc50-cpp) - Single-threaded test
    float random = static_cast<float>(rand1 * 32768 + rand2) / (32768.0f * 32768);
    vrand.push_back(random);
  }
}

int main(int argc, char** argv)
{
  std::cout << "In main" << '\n';
  // Deliberately use C-style random number generation for simplicity in a test
  // NOLINTNEXTLINE(bugprone-random-generator-seed, cert-msc32-c, cert-msc51-cpp)
  srand(time(nullptr));

  std::string const filename = (argc > 1) ? argv[1] : "toy.root";
  std::string const checksum_filename = (argc > 2) ? argv[2] : "toy_checksums.txt";
  int const technology = form::test::getTechnology((argc > 3) ? argv[3] : "ROOT_TTREE");

  // TODO: Read configuration from config file instead of hardcoding
  form::experimental::config::ItemConfig config_items;
  config_items.addItem("trackStart", filename, technology);
  config_items.addItem("trackNumberHits", filename, technology);
  config_items.addItem("trackStartPoints", filename, technology);
  config_items.addItem("trackStartX", filename, technology);

  form::experimental::config::tech_setting_config tech_config;
  tech_config.container_settings[form::technology::ROOT_TTREE]["trackStart"].emplace_back(
    "auto_flush", "1");
  tech_config.file_settings[technology]["toy.root"].emplace_back("compression", "kZSTD");
  tech_config.container_settings[form::technology::ROOT_RNTUPLE]["Toy_Tracker/trackStartPoints"]
    .emplace_back("force_streamer_field", "true");

  {
    form::experimental::form_writer_interface form(config_items, tech_config);

    ToyTracker tracker(4 * 1024);

    // Open checksum file for writing
    std::ofstream checksum_file(checksum_filename);
    if (!checksum_file.is_open()) {
      std::cerr << "ERROR: Could not open checksum file: " << checksum_filename << '\n';
      return 1;
    }

    for (int nevent = 0; nevent < NUMBER_EVENT; nevent++) {
      std::cout << "PHLEX: Write Event No. " << nevent << '\n';

      std::vector<float> track_x;

      for (int nseg = 0; nseg < NUMBER_SEGMENT; nseg++) {

        std::vector<float> track_start_x;
        generate(track_start_x, 4 * 1024 /* * 1024*/); // sub-event processing
        float check = 0.0;
        for (float val : track_start_x)
          check += val;

        std::string const seg_id_text = std::format("[EVENT={:08X};SEG={:08X}]", nevent, nseg);

        std::string const& segment_id = seg_id_text;

        std::vector<form::experimental::product_with_name> products;
        std::string const creator = "Toy_Tracker";

        form::experimental::product_with_name pb = {
          "trackStart", &track_start_x, &typeid(std::vector<float>)};
        products.push_back(pb);

        std::vector<int> track_n_hits(std::from_range, std::views::iota(0, 100));
        for (int val : track_n_hits)
          check += static_cast<float>(val);
        std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                  << ", check = " << check << '\n';

        form::experimental::product_with_name pb_int = {
          "trackNumberHits", &track_n_hits, &typeid(std::vector<int>)};
        products.push_back(pb_int);

        std::vector<TrackStart> start_points = tracker();
        TrackStart checkPoints;
        for (TrackStart const& point : start_points)
          checkPoints += point;
        std::cout << "PHLEX: Segment = " << nseg << ": seg_id_text = " << seg_id_text
                  << ", checkPoints = " << checkPoints << '\n';

        form::experimental::product_with_name pb_points = {
          "trackStartPoints", &start_points, &typeid(std::vector<TrackStart>)};
        products.push_back(pb_points);

        form.write(creator, segment_id, products);

        // Save segment checksums
        checksum_file << std::setprecision(10) << "SEG " << nevent << " " << nseg << " " << check
                      << " " << checkPoints.getX() << " " << checkPoints.getY() << " "
                      << checkPoints.getZ() << "\n";
        track_x.insert(track_x.end(), track_start_x.begin(), track_start_x.end());
      }

      std::cout << "PHLEX: Write Event segments done " << nevent << '\n';

      float check = 0.0;
      for (float val : track_x)
        check += val;

      std::string const evt_id_text = std::format("[EVENT={:08X}]", nevent);

      std::string const& event_id = evt_id_text;

      std::string const creator = "Toy_Tracker_Event";

      form::experimental::product_with_name pb = {
        "trackStartX", &track_x, &typeid(std::vector<float>)};
      std::cout << "PHLEX: Event = " << nevent << ": evt_id_text = " << evt_id_text
                << ", check = " << check << '\n';

      form.write(creator, event_id, pb);

      // Save event checksum
      checksum_file << std::setprecision(10) << "EVT " << nevent << " " << check << "\n";
      std::cout << "PHLEX: Write Event done " << nevent << '\n';
    }

    checksum_file.close();
    std::cout << "PHLEX: Write done. Checksums saved to " << checksum_filename << '\n';

    // Finalize to write FileCatalog metadata
    form.finalize();
    std::cout << "PHLEX: Finalize done. FileCatalog written to file." << '\n';
  }

  // Verify that the generated FileUUID exists and is a canonical UUID string.
  std::unique_ptr<TFile> root_file(TFile::Open(filename.c_str(), "READ"));

  if (root_file == nullptr || root_file->IsZombie()) {
    std::cerr << "ERROR: Could not open generated ROOT file for validation: " << filename << '\n';
    return 1;
  }

  TTree* catalog = root_file->Get<TTree>("FileCatalog");
  if (catalog == nullptr) {
    std::cerr << "ERROR: FileCatalog tree not found in generated ROOT file." << '\n';
    return 1;
  }

  std::string* fileUUID = nullptr;
  int fileFormatVersion = -1;
  catalog->SetBranchAddress("FileUUID", &fileUUID);
  catalog->SetBranchAddress("FileFormatVersion", &fileFormatVersion);
  if (catalog->GetEntries() < 1) {
    std::cerr << "ERROR: FileCatalog tree has no entries." << '\n';
    return 1;
  }
  catalog->GetEntry(0);
  if (fileUUID == nullptr) {
    std::cerr << "ERROR: FileUUID branch did not populate a valid pointer." << '\n';
    return 1;
  }
  std::string fileUUIDValue = *fileUUID;

  TUUID uuidObj(fileUUIDValue.c_str());
  if (uuidObj == TUUID()) {
    std::cerr << "ERROR: FileUUID is not valid: " << fileUUIDValue << '\n';
    return 1;
  }

  std::cout << "PHLEX: FileUUID validated: " << fileUUIDValue << " (version=" << fileFormatVersion
            << ")" << '\n';

  TTree* registry = root_file->Get<TTree>("ProductRegistry");
  if (registry == nullptr) {
    std::cerr << "ERROR: ProductRegistry tree not found in generated ROOT file." << '\n';
    return 1;
  }

  std::string* productName = nullptr;
  std::string* processName = nullptr;
  std::string* producer = nullptr;
  std::string* productID = nullptr;
  registry->SetBranchAddress("ProductName", &productName);
  registry->SetBranchAddress("ProcessName", &processName);
  registry->SetBranchAddress("Producer", &producer);
  registry->SetBranchAddress("ProductID", &productID);

  if (registry->GetEntries() == 0) {
    std::cerr << "ERROR: ProductRegistry tree has no entries." << '\n';
    return 1;
  }

  std::set<std::string> product_names;
  std::set<std::string> product_ids;
  for (int entry = 0; entry < registry->GetEntries(); ++entry) {
    registry->GetEntry(entry);
    if (productName == nullptr || processName == nullptr || producer == nullptr ||
        productID == nullptr) {
      std::cerr << "ERROR: ProductRegistry branches did not populate valid pointers." << '\n';
      return 1;
    }
    std::string const expected_product_id = *productName + "|" + *producer + "|" + *processName;
    if (*productID != expected_product_id) {
      std::cerr << "ERROR: ProductRegistry ProductID mismatch. expected='" << expected_product_id
                << "' got='" << *productID << "'." << '\n';
      return 1;
    }
    product_names.insert(*productName);
    product_ids.insert(*productID);
    std::cout << "PHLEX: ProductRegistry entry: ProductName='" << *productName << "' ProcessName='"
              << *processName << "' Producer='" << *producer << "' ProductID='" << *productID
              << "'\n";
  }

  if (product_names.empty()) {
    std::cerr << "ERROR: ProductRegistry tree contains no product names." << '\n';
    return 1;
  }

  std::cout << "PHLEX: ProductRegistry validated: ";
  for (auto const& name : product_names)
    std::cout << name << " ";
  std::cout << '\n';

  TTree* index_registry = root_file->Get<TTree>("IndexRegistry");
  if (index_registry == nullptr) {
    std::cerr << "ERROR: IndexRegistry tree not found in generated ROOT file." << '\n';
    return 1;
  }

  TObjString* layer_schema_meta = nullptr;
  auto* user_info = index_registry->GetUserInfo();
  if (user_info != nullptr) {
    for (int i = 0; i < user_info->GetEntries(); ++i) {
      auto* obj = user_info->At(i);
      auto* candidate = dynamic_cast<TObjString*>(obj);
      if (candidate == nullptr) {
        continue;
      }
      std::string payload = candidate->GetString().Data();
      if (payload.size() >= 2 && payload.front() == '[' && payload.back() == ']') {
        layer_schema_meta = candidate;
        break;
      }
    }
  }

  if (layer_schema_meta == nullptr) {
    std::cerr << "ERROR: IndexRegistry header does not contain LayerSchema metadata." << '\n';
    return 1;
  }

  std::vector<std::string> header_schema;
  {
    std::string schema_text = layer_schema_meta->GetString().Data();
    if (schema_text.size() >= 2 && schema_text.front() == '[' && schema_text.back() == ']') {
      schema_text = schema_text.substr(1, schema_text.size() - 2);
    }

    std::size_t start = 0;
    while (start <= schema_text.size()) {
      std::size_t end = schema_text.find(',', start);
      std::string token =
        schema_text.substr(start, end == std::string::npos ? std::string::npos : end - start);
      if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
        token = token.substr(1, token.size() - 2);
      }
      if (!token.empty()) {
        header_schema.push_back(token);
      }
      if (end == std::string::npos) {
        break;
      }
      start = end + 1;
    }
  }

  if (header_schema.empty()) {
    std::cerr << "ERROR: IndexRegistry LayerSchema header is empty." << '\n';
    return 1;
  }

  if (index_registry->GetEntries() == 0) {
    std::cerr << "ERROR: IndexRegistry tree has no entries." << '\n';
    return 1;
  }

  auto* branch_list = index_registry->GetListOfBranches();
  if (branch_list == nullptr) {
    std::cerr << "ERROR: IndexRegistry has no branch list." << '\n';
    return 1;
  }
  if (branch_list->GetEntries() != static_cast<int>(header_schema.size()) + 3) {
    std::cerr << "ERROR: IndexRegistry branch count does not match LayerSchema size + ProductID + "
                 "ContainerName + PayloadRow."
              << '\n';
    return 1;
  }
  for (std::size_t i = 0; i < header_schema.size(); ++i) {
    auto* branch_obj = branch_list->At(static_cast<int>(i));
    if (branch_obj == nullptr) {
      std::cerr << "ERROR: IndexRegistry has null branch object at position " << i << "." << '\n';
      return 1;
    }
    if (header_schema[i] != branch_obj->GetName()) {
      std::cerr << "ERROR: IndexRegistry branch order does not match LayerSchema at position " << i
                << ": expected '" << header_schema[i] << "' got '" << branch_obj->GetName() << "'."
                << '\n';
      return 1;
    }
  }
  auto* product_branch_obj = branch_list->At(static_cast<int>(header_schema.size()));
  if (product_branch_obj == nullptr || std::string(product_branch_obj->GetName()) != "ProductID") {
    std::cerr << "ERROR: IndexRegistry branch order missing ProductID after layer branches."
              << '\n';
    return 1;
  }
  auto* container_branch_obj = branch_list->At(static_cast<int>(header_schema.size()) + 1);
  if (container_branch_obj == nullptr ||
      std::string(container_branch_obj->GetName()) != "ContainerName") {
    std::cerr << "ERROR: IndexRegistry branch order missing ContainerName after ProductID." << '\n';
    return 1;
  }
  auto* payload_row_branch_obj = branch_list->At(static_cast<int>(header_schema.size()) + 2);
  if (payload_row_branch_obj == nullptr ||
      std::string(payload_row_branch_obj->GetName()) != "PayloadRow") {
    std::cerr << "ERROR: IndexRegistry branch order missing PayloadRow after ContainerName."
              << '\n';
    return 1;
  }

  std::vector<unsigned long long> layer_branch_values(header_schema.size(), 0);
  for (std::size_t i = 0; i < header_schema.size(); ++i) {
    if (index_registry->GetBranch(header_schema[i].c_str()) == nullptr) {
      std::cerr << "ERROR: IndexRegistry branch missing for layer '" << header_schema[i] << "'."
                << '\n';
      return 1;
    }
    index_registry->SetBranchAddress(header_schema[i].c_str(), &layer_branch_values[i]);
  }
  std::string* product_id = nullptr;
  if (index_registry->GetBranch("ProductID") == nullptr) {
    std::cerr << "ERROR: IndexRegistry ProductID branch missing." << '\n';
    return 1;
  }
  index_registry->SetBranchAddress("ProductID", &product_id);
  std::string* container_name = nullptr;
  if (index_registry->GetBranch("ContainerName") == nullptr) {
    std::cerr << "ERROR: IndexRegistry ContainerName branch missing." << '\n';
    return 1;
  }
  index_registry->SetBranchAddress("ContainerName", &container_name);
  unsigned long long payload_row = 0;
  if (index_registry->GetBranch("PayloadRow") == nullptr) {
    std::cerr << "ERROR: IndexRegistry PayloadRow branch missing." << '\n';
    return 1;
  }
  index_registry->SetBranchAddress("PayloadRow", &payload_row);

  Long64_t const sample_entries =
    index_registry->GetEntries() < 8 ? index_registry->GetEntries() : 8;
  unsigned long long prev_event = 0;
  unsigned long long prev_seg = 0;
  std::set<std::string> observed_product_ids;
  bool has_prev = false;
  for (int entry = 0; entry < index_registry->GetEntries(); ++entry) {
    index_registry->GetEntry(entry);
    if (layer_branch_values.empty()) {
      std::cerr << "ERROR: IndexRegistry layer branch values are empty." << '\n';
      return 1;
    }
    if (product_id == nullptr || product_id->empty()) {
      std::cerr << "ERROR: IndexRegistry ProductID branch did not populate valid value." << '\n';
      return 1;
    }
    if (container_name == nullptr || container_name->empty()) {
      std::cerr << "ERROR: IndexRegistry ContainerName branch did not populate valid value."
                << '\n';
      return 1;
    }
    // ContainerName must be a top-level TTree/RNTuple name (no '/' slash)
    if (container_name->find('/') != std::string::npos) {
      std::cerr << "ERROR: IndexRegistry ContainerName should be top-level container name, got: '"
                << *container_name << "'." << '\n';
      return 1;
    }

    unsigned long long const expected_payload_row = static_cast<unsigned long long>(entry);
    if (payload_row != expected_payload_row) {
      std::cerr << "ERROR: IndexRegistry PayloadRow mismatch at entry " << entry << ": expected "
                << expected_payload_row << " got " << payload_row << "." << '\n';
      return 1;
    }

    observed_product_ids.insert(*product_id);

    if (entry < sample_entries && header_schema.size() >= 2) {
      unsigned long long const event_val = layer_branch_values[0];
      unsigned long long const seg_val = layer_branch_values[1];

      if (event_val >= static_cast<unsigned long long>(NUMBER_EVENT)) {
        std::cerr << "ERROR: EVENT value out of range in IndexRegistry sample: " << event_val
                  << '\n';
        return 1;
      }
      if (seg_val >= static_cast<unsigned long long>(NUMBER_SEGMENT)) {
        std::cerr << "ERROR: SEG value out of range in IndexRegistry sample: " << seg_val << '\n';
        return 1;
      }

      if (has_prev) {
        bool const monotonic_ok = (event_val > prev_event && seg_val == 0) ||
                                  (event_val == prev_event && seg_val > prev_seg) ||
                                  (event_val == prev_event && seg_val == prev_seg);
        if (!monotonic_ok) {
          std::cerr
            << "ERROR: IndexRegistry sample entries are not monotonic by EVENT/SEG ordering."
            << '\n';
          return 1;
        }
      }

      prev_event = event_val;
      prev_seg = seg_val;
      has_prev = true;
    }
  }

  if (header_schema.size() != 2 || header_schema[0] != "EVENT" || header_schema[1] != "SEG") {
    std::cerr
      << "ERROR: IndexRegistry LayerSchema header is not EVENT,SEG as expected for this test."
      << '\n';
    return 1;
  }

  if (product_ids.empty()) {
    std::cerr << "ERROR: ProductRegistry ProductID values are empty." << '\n';
    return 1;
  }

  if (observed_product_ids.empty()) {
    std::cerr << "ERROR: IndexRegistry ProductID values are empty." << '\n';
    return 1;
  }

  for (auto const& observed_id : observed_product_ids) {
    if (!product_ids.contains(observed_id)) {
      std::cerr << "ERROR: IndexRegistry ProductID value not present in ProductRegistry: "
                << observed_id << '\n';
      return 1;
    }
  }

  std::cout << "PHLEX: ProductRegistry validated with " << product_names.size()
            << " product names and " << product_ids.size() << " product IDs" << '\n';

  std::cout << "PHLEX: IndexRegistry validated with " << index_registry->GetEntries() << " entries"
            << '\n';

  return 0;
}
