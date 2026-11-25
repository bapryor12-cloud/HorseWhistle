// Writes HorseWhistle.esp. Usage: dotnet run -- <output.esp>
using Mutagen.Bethesda;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Skyrim;

var skyrim = ModKey.FromFileName("Skyrim.esm");
FormKey Vanilla(uint id) => new(skyrim, id);

var mod = new SkyrimMod(ModKey.FromFileName("HorseWhistle.esp"), SkyrimRelease.SkyrimSE);
mod.ModHeader.Flags |= SkyrimModHeader.HeaderFlag.Small;
mod.ModHeader.Author = "bapryor";

// The whistle is a reusable "potion": the DLL gives it back after use and calls the horse.
var effect = mod.MagicEffects.AddNew("HW_CallHorseEffect");
effect.Name = "Call Horse";
effect.Description = "Calls your horse to your side, if it is alive and you are outdoors.";
effect.Archetype = new MagicEffectArchetype(MagicEffectArchetype.TypeEnum.Script);
effect.CastType = CastType.FireAndForget;
effect.TargetType = TargetType.Self;
effect.Flags = MagicEffect.Flag.NoMagnitude | MagicEffect.Flag.NoDuration | MagicEffect.Flag.NoArea | MagicEffect.Flag.Recover;
effect.MagicSkill = ActorValue.None;
effect.ResistValue = ActorValue.None;
effect.SecondActorValue = ActorValue.None;

var whistle = mod.Ingestibles.AddNew("HW_HorseWhistle");
whistle.Name = "Horse Whistle";
whistle.ObjectBounds = new ObjectBounds { First = new(-2, -12, -2), Second = new(2, 12, 2) };
whistle.Model = new Model { File = @"Clutter\Instruments\Flute01.nif" };  // the DLL swaps in the vanilla flute's model
whistle.Weight = 0.5f;
whistle.Value = 25;
whistle.Flags = Ingestible.Flag.NoAutoCalc;
whistle.Keywords = new() { Vanilla(0x914EE).ToLink<IKeywordGetter>() };  // VendorItemTool
whistle.Effects.Add(new Effect { BaseEffect = effect.ToNullableLink(), Data = new EffectData() });

// Dialogue with the five stablemasters.
var quest = mod.Quests.AddNew("HW_WhistleQuest");
quest.Name = "Horse Whistle Seller";
quest.Flags = Quest.Flag.StartGameEnabled;
quest.Priority = 50;

var topic = mod.DialogTopics.AddNew("HW_BuyWhistleTopic");
var branch = mod.DialogBranches.AddNew("HW_BuyWhistleBranch");
branch.Quest.SetTo(quest);
branch.Category = 0;  // Player; vanilla branches always write TNAM
branch.Flags = DialogBranch.Flag.TopLevel;
branch.StartingTopic.SetTo(topic);

topic.Name = "I'd like to buy a horse whistle. (25 gold)";
topic.Quest.SetTo(quest);
topic.Branch.SetTo(branch);
topic.Category = DialogTopic.CategoryEnum.Topic;
topic.Subtype = DialogTopic.SubtypeEnum.Custom;
topic.SubtypeName = new RecordType("CUST");  // the game reads the subtype from SNAM; left empty it crashes on load
topic.Priority = 50;

var info = new DialogResponses(mod.GetNextFormKey(), SkyrimRelease.SkyrimSE)
{
    Flags = new DialogResponseFlags(),
};
info.Responses.Add(new DialogResponse
{
    Text = "Here you go. Blow it anywhere outdoors and your horse will come running.",
    Emotion = Emotion.Neutral,
    ResponseNumber = 1,
});

Condition IsSpeaker(uint npc, bool or) => new ConditionFloat
{
    CompareOperator = CompareOperator.EqualTo,
    ComparisonValue = 1,
    Flags = or ? Condition.Flag.OR : 0,
    Data = IsID(npc),
};
GetIsIDConditionData IsID(uint npc)
{
    var data = new GetIsIDConditionData();
    data.Object.Link.SetTo(Vanilla(npc));
    return data;
}
uint[] stablemasters = { 0x13BB7 /* Skulvar */, 0x14141 /* Ulundil */, 0x1338F /* Cedran */, 0x13351 /* Hofgrir */, 0x1327D /* Geimund */ };
for (int i = 0; i < stablemasters.Length; i++) {
    info.Conditions.Add(IsSpeaker(stablemasters[i], i < stablemasters.Length - 1));
}
var goldCheck = new GetItemCountConditionData();
goldCheck.ItemOrList.Link.SetTo(Vanilla(0xF));  // Gold001
goldCheck.RunOnType = Condition.RunOnType.Reference;
goldCheck.Reference.SetTo(new FormKey(skyrim, 0x14));  // the player
info.Conditions.Add(new ConditionFloat { CompareOperator = CompareOperator.GreaterThanOrEqualTo, ComparisonValue = 25, Data = goldCheck });
topic.Responses.Add(info);

mod.WriteToBinary(args[0], new Mutagen.Bethesda.Plugins.Binary.Parameters.BinaryWriteParameters
{
    MastersListContent = Mutagen.Bethesda.Plugins.Binary.Parameters.MastersListContentOption.Iterate,
});
Console.WriteLine($"Wrote {args[0]}; whistle {whistle.FormKey}, info {info.FormKey}");
