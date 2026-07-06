// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "UI/WTBRBagLootWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WTBRBagLootViewModelComponent.h"
#include "Engine/Texture2D.h"
#include "WTBRCharacter.h"

void UWTBRBagLootWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (UWTBRBagLootViewModelComponent* ViewModel = ResolveViewModel())
    {
        BoundViewModel = ViewModel;
        ViewModel->OnBagLootSnapshotChanged.AddUniqueDynamic(this, &UWTBRBagLootWidget::OnViewModelSnapshotChanged);
    }

    RefreshFromViewModel();
}

void UWTBRBagLootWidget::NativeDestruct()
{
    if (UWTBRBagLootViewModelComponent* ViewModel = BoundViewModel.Get())
    {
        ViewModel->OnBagLootSnapshotChanged.RemoveDynamic(this, &UWTBRBagLootWidget::OnViewModelSnapshotChanged);
    }
    BoundViewModel.Reset();

    Super::NativeDestruct();
}

UWTBRBagLootViewModelComponent* UWTBRBagLootWidget::ResolveViewModel() const
{
    const AWTBRCharacter* Character = Cast<AWTBRCharacter>(GetOwningPlayerPawn());
    return IsValid(Character) ? Character->GetBagLootViewModelComponent() : nullptr;
}

void UWTBRBagLootWidget::OnViewModelSnapshotChanged()
{
    RefreshFromViewModel();
}

void UWTBRBagLootWidget::RefreshFromViewModel()
{
    UWTBRBagLootViewModelComponent* ViewModel = BoundViewModel.Get();
    if (!IsValid(ViewModel))
    {
        // Owning pawn may not have been valid yet at NativeConstruct time
        // (e.g. widget created before possession completes) — try again now.
        ViewModel = ResolveViewModel();
        if (IsValid(ViewModel))
        {
            BoundViewModel = ViewModel;
            ViewModel->OnBagLootSnapshotChanged.AddUniqueDynamic(this, &UWTBRBagLootWidget::OnViewModelSnapshotChanged);
        }
    }

    if (!IsValid(ViewModel))
    {
        return;
    }

    ApplySnapshot(ViewModel->GetBagLootSnapshot());
}

void UWTBRBagLootWidget::ApplySnapshot(const FWTBRBagLootSnapshot& Snapshot)
{
    ApplyBagSnapshot(Snapshot.Bag);
    ApplyCorpseLootSnapshot(Snapshot.CorpseLoot);

    // Txt_ContextHint (WBP_BagLootLayer_Generated only) has no dedicated snapshot
    // field — left as authored in the WBP, not populated here (same treatment as
    // static labels like Txt_BagTitle/Txt_BagHint below).
}

void UWTBRBagLootWidget::ApplyBagSnapshot(const FWTBRBagPanelSnapshot& Bag)
{
    // Txt_BagTitle / Txt_BagHint are static labels with no corresponding snapshot
    // field (same treatment as e.g. Txt_HPLabel in UWTBRGeneratedHUDWidget) — left
    // as authored in the WBP, not touched here.
    SetTextSafe(Txt_BagCapacity, Bag.CapacityText);
    // Always the ViewModel's neutral placeholder ("-") — no weight/volume system
    // exists anywhere in UWTBRInventoryComponent (see BindingPlan §1/§5). Never
    // fabricate a number here; just forward whatever the snapshot already decided.
    SetTextSafe(Txt_BagWeight, Bag.WeightText);
    SetTextSafe(Txt_BagWarning, Bag.WarningText);

    for (int32 SlotIndex = 0; SlotIndex < 14; ++SlotIndex)
    {
        UTextBlock* NameWidget = nullptr;
        UTextBlock* CountWidget = nullptr;
        UImage* IconWidget = nullptr;
        if (!GetSlotTextWidgets(SlotIndex, NameWidget, CountWidget, IconWidget))
        {
            continue;
        }

        const FWTBRBagSlotSnapshot* SlotSnapshot = Bag.Slots.IsValidIndex(SlotIndex) ? &Bag.Slots[SlotIndex] : nullptr;
        if (SlotSnapshot && SlotSnapshot->bHasItem)
        {
            SetTextSafe(NameWidget, SlotSnapshot->DisplayName);
            SetTextSafe(CountWidget, SlotSnapshot->CountText);
            // Non-blocking resolve only — never forces a synchronous load during
            // a display refresh (mirrors the ViewModel's own rule).
            SetImageSafe(IconWidget, SlotSnapshot->Icon.Get());
        }
        else
        {
            SetTextSafe(NameWidget, FText::GetEmpty());
            SetTextSafe(CountWidget, FText::GetEmpty());
            ClearImageSafe(IconWidget);
        }
    }
}

void UWTBRBagLootWidget::ApplyCorpseLootSnapshot(const FWTBRCorpseLootPanelSnapshot& CorpseLoot)
{
    if (CorpseLoot.bHasFocusedContainer)
    {
        SetTextSafe(Txt_CorpseLootTitle, CorpseLoot.TitleText);
        SetTextSafe(Txt_CorpseLootSubtitle, CorpseLoot.SubtitleText);
        SetTextSafe(Txt_CorpseLootPrompt, CorpseLoot.PromptText);
    }
    else
    {
        // No focused container — keep panel data safe/empty, do not invent fake loot.
        SetTextSafe(Txt_CorpseLootTitle, FText::GetEmpty());
        SetTextSafe(Txt_CorpseLootSubtitle, FText::GetEmpty());
        SetTextSafe(Txt_CorpseLootPrompt, FText::GetEmpty());
    }

    for (int32 RowIndex = 0; RowIndex < 8; ++RowIndex)
    {
        UTextBlock* NameWidget = nullptr;
        UTextBlock* TypeWidget = nullptr;
        UTextBlock* ActionWidget = nullptr;
        UImage* IconWidget = nullptr;
        if (!GetLootEntryTextWidgets(RowIndex, NameWidget, TypeWidget, ActionWidget, IconWidget))
        {
            continue;
        }

        const FWTBRLootEntrySnapshot* EntrySnapshot = CorpseLoot.Entries.IsValidIndex(RowIndex) ? &CorpseLoot.Entries[RowIndex] : nullptr;
        if (EntrySnapshot && EntrySnapshot->bHasEntry)
        {
            SetTextSafe(NameWidget, EntrySnapshot->DisplayName);
            SetTextSafe(TypeWidget, EntrySnapshot->TypeText);
            SetTextSafe(ActionWidget, EntrySnapshot->ActionText);
            SetImageSafe(IconWidget, EntrySnapshot->Icon.Get());
        }
        else
        {
            SetTextSafe(NameWidget, FText::GetEmpty());
            SetTextSafe(TypeWidget, FText::GetEmpty());
            SetTextSafe(ActionWidget, FText::GetEmpty());
            ClearImageSafe(IconWidget);
        }
    }
}

bool UWTBRBagLootWidget::RequestUseBagItem(int32 SlotIndex)
{
    UWTBRBagLootViewModelComponent* ViewModel = BoundViewModel.IsValid() ? BoundViewModel.Get() : ResolveViewModel();
    if (!IsValid(ViewModel))
    {
        return false;
    }

    return ViewModel->RequestUseBagItem(SlotIndex);
}

bool UWTBRBagLootWidget::RequestPickupCorpseLootEntry(int32 LootIndex, int32 TargetSlotIndex)
{
    UWTBRBagLootViewModelComponent* ViewModel = BoundViewModel.IsValid() ? BoundViewModel.Get() : ResolveViewModel();
    if (!IsValid(ViewModel))
    {
        return false;
    }

    return ViewModel->RequestPickupCorpseLootEntry(LootIndex, TargetSlotIndex);
}

bool UWTBRBagLootWidget::GetSlotTextWidgets(int32 SlotIndex, UTextBlock*& OutName, UTextBlock*& OutCount, UImage*& OutIcon) const
{
    OutName = nullptr;
    OutCount = nullptr;
    OutIcon = nullptr;

    switch (SlotIndex)
    {
    case 0:  OutName = Txt_BagSlot01Name; OutCount = Txt_BagSlot01Count; OutIcon = Img_BagSlot01Icon; break;
    case 1:  OutName = Txt_BagSlot02Name; OutCount = Txt_BagSlot02Count; OutIcon = Img_BagSlot02Icon; break;
    case 2:  OutName = Txt_BagSlot03Name; OutCount = Txt_BagSlot03Count; OutIcon = Img_BagSlot03Icon; break;
    case 3:  OutName = Txt_BagSlot04Name; OutCount = Txt_BagSlot04Count; OutIcon = Img_BagSlot04Icon; break;
    case 4:  OutName = Txt_BagSlot05Name; OutCount = Txt_BagSlot05Count; OutIcon = Img_BagSlot05Icon; break;
    case 5:  OutName = Txt_BagSlot06Name; OutCount = Txt_BagSlot06Count; OutIcon = Img_BagSlot06Icon; break;
    case 6:  OutName = Txt_BagSlot07Name; OutCount = Txt_BagSlot07Count; OutIcon = Img_BagSlot07Icon; break;
    case 7:  OutName = Txt_BagSlot08Name; OutCount = Txt_BagSlot08Count; OutIcon = Img_BagSlot08Icon; break;
    case 8:  OutName = Txt_BagSlot09Name; OutCount = Txt_BagSlot09Count; OutIcon = Img_BagSlot09Icon; break;
    case 9:  OutName = Txt_BagSlot10Name; OutCount = Txt_BagSlot10Count; OutIcon = Img_BagSlot10Icon; break;
    case 10: OutName = Txt_BagSlot11Name; OutCount = Txt_BagSlot11Count; OutIcon = Img_BagSlot11Icon; break;
    case 11: OutName = Txt_BagSlot12Name; OutCount = Txt_BagSlot12Count; OutIcon = Img_BagSlot12Icon; break;
    case 12: OutName = Txt_BagSlot13Name; OutCount = Txt_BagSlot13Count; OutIcon = Img_BagSlot13Icon; break;
    case 13: OutName = Txt_BagSlot14Name; OutCount = Txt_BagSlot14Count; OutIcon = Img_BagSlot14Icon; break;
    default: return false;
    }

    return true;
}

bool UWTBRBagLootWidget::GetLootEntryTextWidgets(int32 RowIndex, UTextBlock*& OutName, UTextBlock*& OutType, UTextBlock*& OutAction, UImage*& OutIcon) const
{
    OutName = nullptr;
    OutType = nullptr;
    OutAction = nullptr;
    OutIcon = nullptr;

    switch (RowIndex)
    {
    case 0: OutName = Txt_LootEntry01Name; OutType = Txt_LootEntry01Type; OutAction = Txt_LootEntry01Action; OutIcon = Img_LootEntry01Icon; break;
    case 1: OutName = Txt_LootEntry02Name; OutType = Txt_LootEntry02Type; OutAction = Txt_LootEntry02Action; OutIcon = Img_LootEntry02Icon; break;
    case 2: OutName = Txt_LootEntry03Name; OutType = Txt_LootEntry03Type; OutAction = Txt_LootEntry03Action; OutIcon = Img_LootEntry03Icon; break;
    case 3: OutName = Txt_LootEntry04Name; OutType = Txt_LootEntry04Type; OutAction = Txt_LootEntry04Action; OutIcon = Img_LootEntry04Icon; break;
    case 4: OutName = Txt_LootEntry05Name; OutType = Txt_LootEntry05Type; OutAction = Txt_LootEntry05Action; OutIcon = Img_LootEntry05Icon; break;
    case 5: OutName = Txt_LootEntry06Name; OutType = Txt_LootEntry06Type; OutAction = Txt_LootEntry06Action; OutIcon = Img_LootEntry06Icon; break;
    case 6: OutName = Txt_LootEntry07Name; OutType = Txt_LootEntry07Type; OutAction = Txt_LootEntry07Action; OutIcon = Img_LootEntry07Icon; break;
    case 7: OutName = Txt_LootEntry08Name; OutType = Txt_LootEntry08Type; OutAction = Txt_LootEntry08Action; OutIcon = Img_LootEntry08Icon; break;
    default: return false;
    }

    return true;
}

void UWTBRBagLootWidget::SetTextSafe(UTextBlock* TextBlock, const FText& Text)
{
    if (IsValid(TextBlock))
    {
        TextBlock->SetText(Text);
    }
}

void UWTBRBagLootWidget::SetImageSafe(UImage* Image, UTexture2D* Texture)
{
    if (!IsValid(Image))
    {
        return;
    }

    if (IsValid(Texture))
    {
        Image->SetBrushFromTexture(Texture, false);
        Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        ClearImageSafe(Image);
    }
}

void UWTBRBagLootWidget::ClearImageSafe(UImage* Image)
{
    if (IsValid(Image))
    {
        Image->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UWTBRBagLootWidget::SetVisibilitySafe(UWidget* Widget, ESlateVisibility Visibility)
{
    if (IsValid(Widget))
    {
        Widget->SetVisibility(Visibility);
    }
}
