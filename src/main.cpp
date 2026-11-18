// Horse Whistle
//
// HorseWhistle.esp adds a "Horse Whistle" (a reusable potion-type item with the
// vanilla flute's model) and a dialogue line to buy one from the five
// stablemasters for 25 gold. This plugin:
//  - charges the gold and hands over the whistle when that line is chosen,
//  - gives the whistle back whenever it's used, and calls the player's horse:
//    if the player owns a horse that's alive, it appears in front of them.

namespace
{
	constexpr std::string_view kPluginFile = "HorseWhistle.esp";
	constexpr RE::FormID       kWhistleID = 0x801;
	constexpr RE::FormID       kBuyInfoID = 0x805;
	constexpr std::int32_t     kPrice = 25;

	// Skyrim.esm
	constexpr RE::FormID kFlute = 0xDABA7;
	constexpr RE::FormID kGold = 0xF;
	constexpr RE::FormID kPlayerHorseFaction = 0x68D78;
	constexpr RE::FormID kPlayerFaction = 0xDB1;

	constexpr float kSpawnDistance = 300.0f;  // in front of the player

	constexpr std::uint32_t kCosaveID = 'HWHS';
	constexpr std::uint32_t kCosaveVersion = 1;
	constexpr std::uint32_t kRecordHorse = 'HRSE';

	RE::AlchemyItem* g_whistle = nullptr;
	RE::FormID       g_buyInfo = 0;
	RE::TESBoundObject* g_gold = nullptr;
	RE::TESFaction*  g_playerHorseFaction = nullptr;
	RE::TESFaction*  g_playerFaction = nullptr;

	// The last horse the player rode that they own. Saved per save game.
	RE::FormID g_horse = 0;

	void SetupLog()
	{
		auto dir = SKSE::log::log_directory();
		if (!dir) {
			return;
		}
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((*dir / "HorseWhistle.log").string(), true);
		auto logger = std::make_shared<spdlog::logger>("log", std::move(sink));
		logger->set_level(spdlog::level::info);
		logger->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(logger));
		spdlog::set_pattern("[%H:%M:%S] %v");
	}

	void Notify(const std::string& a_text)
	{
		RE::SendHUDMessage::ShowHUDMessage(a_text.c_str());
	}

	std::int32_t GoldCount(RE::PlayerCharacter* a_player)
	{
		const auto counts = a_player->GetInventoryCounts([](RE::TESBoundObject& a_obj) { return &a_obj == g_gold; });
		const auto it = counts.find(g_gold);
		return it != counts.end() ? it->second : 0;
	}

	bool IsOwnedByPlayer(RE::Actor* a_horse)
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (g_playerHorseFaction && a_horse->IsInFaction(g_playerHorseFaction)) {
			return true;
		}
		const auto owner = a_horse->GetOwner();
		return owner && (owner == player->GetActorBase() || (g_playerFaction && owner == g_playerFaction));
	}

	// The horse to call: the last one ridden if the player owns it, otherwise the
	// last owned horse remembered from earlier.
	RE::Actor* FindHorse()
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (auto ridden = player->GetPlayerRuntimeData().lastRiddenMount.get(); ridden && IsOwnedByPlayer(ridden.get())) {
			g_horse = ridden->GetFormID();
			return ridden.get();
		}
		return g_horse ? RE::TESForm::LookupByID<RE::Actor>(g_horse) : nullptr;
	}

	void CallHorse()
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		auto cell = player->GetParentCell();
		if (!cell || cell->IsInteriorCell()) {
			Notify("Your horse can't reach you here.");
			return;
		}
		if (player->IsOnMount()) {
			Notify("You're already riding.");
			return;
		}
		auto horse = FindHorse();
		if (!horse) {
			Notify("You have no horse to call.");
			return;
		}
		if (horse->IsDead()) {
			Notify(std::format("{} is dead.", horse->GetName()));
			return;
		}

		const float heading = player->GetAngleZ();
		RE::NiPoint3 position = player->GetPosition();
		position.x += std::sin(heading) * kSpawnDistance;
		position.y += std::cos(heading) * kSpawnDistance;
		RE::NiPoint3 rotation{ 0.0f, 0.0f, heading + std::numbers::pi_v<float> };  // facing the player

		// Bring it to the player's cell, then step it out in front. The position is
		// set again a frame later in case the move finished asynchronously.
		horse->MoveTo(player);
		horse->SetPosition(position, true);
		horse->SetAngle(rotation);
		const auto horseID = horse->GetFormID();
		SKSE::GetTaskInterface()->AddTask([horseID, position, rotation]() {
			if (auto horse = RE::TESForm::LookupByID<RE::Actor>(horseID)) {
				horse->SetPosition(position, true);
				horse->SetAngle(rotation);
			}
		});
		spdlog::info("Called horse {} [{:08X}]", horse->GetName(), horse->GetFormID());
	}

	void SellWhistle()
	{
		auto player = RE::PlayerCharacter::GetSingleton();
		if (!g_whistle || !g_gold || GoldCount(player) < kPrice) {
			return;
		}
		player->RemoveItem(g_gold, kPrice, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
		player->AddObjectToContainer(g_whistle, nullptr, 1, nullptr);
		Notify(std::format("{} gold removed. Horse Whistle added.", kPrice));
		spdlog::info("Sold a horse whistle");
	}

	class EventHandler :
		public RE::BSTEventSink<RE::TESEquipEvent>,
		public RE::BSTEventSink<RE::TESTopicInfoEvent>
	{
	public:
		static EventHandler* GetSingleton()
		{
			static EventHandler singleton;
			return &singleton;
		}

		// Using the whistle "drinks" it: give it back and call the horse.
		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
		{
			if (a_event && a_event->equipped && g_whistle && a_event->baseObject == g_whistle->GetFormID() &&
				a_event->actor && a_event->actor->IsPlayerRef()) {
				SKSE::GetTaskInterface()->AddTask([]() {
					RE::PlayerCharacter::GetSingleton()->AddObjectToContainer(g_whistle, nullptr, 1, nullptr);
					CallHorse();
				});
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESTopicInfoEvent* a_event, RE::BSTEventSource<RE::TESTopicInfoEvent>*) override
		{
			if (a_event && g_buyInfo && a_event->topicInfoFormID == g_buyInfo &&
				a_event->type.all(RE::TESTopicInfoEvent::TopicInfoEventType::kTopicBegin)) {
				SKSE::GetTaskInterface()->AddTask(SellWhistle);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	void OnDataLoaded()
	{
		auto dataHandler = RE::TESDataHandler::GetSingleton();
		g_whistle = dataHandler->LookupForm<RE::AlchemyItem>(kWhistleID, kPluginFile);
		g_buyInfo = dataHandler->LookupFormID(kBuyInfoID, kPluginFile);
		g_gold = dataHandler->LookupForm<RE::TESBoundObject>(kGold, "Skyrim.esm");
		g_playerHorseFaction = dataHandler->LookupForm<RE::TESFaction>(kPlayerHorseFaction, "Skyrim.esm");
		g_playerFaction = dataHandler->LookupForm<RE::TESFaction>(kPlayerFaction, "Skyrim.esm");

		if (!g_whistle) {
			spdlog::error("{} is not loaded; the whistle won't work", kPluginFile);
			return;
		}
		// Look like the vanilla flute.
		if (auto flute = dataHandler->LookupForm<RE::TESObjectMISC>(kFlute, "Skyrim.esm")) {
			static_cast<RE::TESModelTextureSwap*>(g_whistle)->CopyComponent(static_cast<RE::TESModelTextureSwap*>(flute));
		}
		spdlog::info("Whistle {:08X}, buy line {:08X}", g_whistle->GetFormID(), g_buyInfo);

		auto events = RE::ScriptEventSourceHolder::GetSingleton();
		events->AddEventSink<RE::TESEquipEvent>(EventHandler::GetSingleton());
		events->AddEventSink<RE::TESTopicInfoEvent>(EventHandler::GetSingleton());
	}

	void OnSave(SKSE::SerializationInterface* a_intfc)
	{
		if (a_intfc->OpenRecord(kRecordHorse, kCosaveVersion)) {
			a_intfc->WriteRecordData(g_horse);
		}
	}

	void OnLoad(SKSE::SerializationInterface* a_intfc)
	{
		std::uint32_t type, version, length;
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type == kRecordHorse && version == kCosaveVersion) {
				RE::FormID horse = 0;
				a_intfc->ReadRecordData(horse);
				RE::FormID resolved = 0;
				g_horse = horse && a_intfc->ResolveFormID(horse, resolved) ? resolved : 0;
			}
		}
	}

	void OnRevert(SKSE::SerializationInterface*)
	{
		g_horse = 0;
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
			OnDataLoaded();
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SetupLog();
	SKSE::Init(a_skse);
	spdlog::info("Horse Whistle loaded");

	auto serialization = SKSE::GetSerializationInterface();
	serialization->SetUniqueID(kCosaveID);
	serialization->SetSaveCallback(OnSave);
	serialization->SetLoadCallback(OnLoad);
	serialization->SetRevertCallback(OnRevert);

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		spdlog::error("Failed to register message listener");
		return false;
	}
	return true;
}
