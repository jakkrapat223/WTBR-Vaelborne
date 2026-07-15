// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "UI/WTBRRadarWidget.h"
#include "WTBRCharacter.h"
#include "Components/WTBRHealthComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"

TSharedRef<SWidget> UWTBRRadarWidget::RebuildWidget()
{
    return SNew(SBox);
}

void UWTBRRadarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    ScanAccumulator += InDeltaTime;
    if (ScanAccumulator < FMath::Max(ContactScanInterval, 0.02f)) return;
    ScanAccumulator = 0.0f;
    RefreshContacts();
}

void UWTBRRadarWidget::RefreshContacts()
{
    const APlayerController* PC = GetOwningPlayer();
    const AWTBRCharacter* LocalCharacter = PC ? Cast<AWTBRCharacter>(PC->GetPawn()) : nullptr;
    UWorld* World = GetWorld();
    if (!IsValid(LocalCharacter) || !World)
    {
        Contacts.Empty();
        return;
    }

    const float Now = World->GetTimeSeconds();
    const float MinimumTrailMoveSquared = FMath::Square(50.0f);
    TSet<TWeakObjectPtr<AWTBRCharacter>> VisibleContacts;

    for (TActorIterator<AWTBRCharacter> It(World); It; ++It)
    {
        AWTBRCharacter* Candidate = *It;
        if (!IsValid(Candidate) || Candidate == LocalCharacter || Candidate->IsRadarCloaked()
            || !IsValid(Candidate->HealthComponent) || Candidate->HealthComponent->IsEliminated()) continue;

        const FVector Delta = Candidate->GetActorLocation() - LocalCharacter->GetActorLocation();
        if (FVector2D(Delta.X, Delta.Y).SizeSquared() > FMath::Square(RadarRange)) continue;

        const TWeakObjectPtr<AWTBRCharacter> Key(Candidate);
        VisibleContacts.Add(Key);
        FRadarContact& Contact = Contacts.FindOrAdd(Key);
        Contact.Character = Candidate;
        Contact.Location = Candidate->GetActorLocation();
        Contact.bFriendly = Candidate->IsSameTeamAs(LocalCharacter);

        if (Contact.Trail.IsEmpty()
            || FVector::DistSquared(Contact.Trail.Last().Location, Contact.Location) >= MinimumTrailMoveSquared)
        {
            Contact.Trail.Add({ Contact.Location, Now });
        }
        Contact.Trail.RemoveAll([Now, this](const FRadarTrailSample& Sample)
        {
            return Sample.TimeSeconds < Now - TrailDuration;
        });
    }

    for (auto It = Contacts.CreateIterator(); It; ++It)
    {
        if (!VisibleContacts.Contains(It.Key()))
        {
            It.RemoveCurrent();
        }
    }
}

int32 UWTBRRadarWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
        LayerId, InWidgetStyle, bParentEnabled);

    const APlayerController* PC = GetOwningPlayer();
    const AWTBRCharacter* LocalCharacter = PC ? Cast<AWTBRCharacter>(PC->GetPawn()) : nullptr;
    if (!IsValid(LocalCharacter) || !GetWorld()) return LayerId;

    const FVector2D ScreenSize = AllottedGeometry.GetLocalSize();
    const float Diameter = 180.0f;
    const FVector2D TopLeft(FMath::Max(16.0f, ScreenSize.X - Diameter - 24.0f), 24.0f);
    const FVector2D Center = TopLeft + FVector2D(Diameter * 0.5f, Diameter * 0.5f);
    const float Radius = Diameter * 0.5f;
    const FSlateBrush* WhiteBrush = FAppStyle::Get().GetBrush("WhiteBrush");

    FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
        AllottedGeometry.ToPaintGeometry(TopLeft, FVector2D(Diameter, Diameter)), WhiteBrush,
        ESlateDrawEffect::None, FLinearColor(0.02f, 0.05f, 0.08f, 0.86f));

    // Canon-style concentric range rings plus a heading crosshair.
    for (int32 RingIndex = 1; RingIndex <= 3; ++RingIndex)
    {
        TArray<FVector2D> Ring;
        const float RingRadius = Radius * static_cast<float>(RingIndex) / 3.0f;
        for (int32 Index = 0; Index <= 32; ++Index)
        {
            const float Angle = (2.0f * PI * Index) / 32.0f;
            Ring.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius);
        }
        const bool bOuterRing = RingIndex == 3;
        FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(), Ring,
            ESlateDrawEffect::None,
            bOuterRing ? FLinearColor(0.35f, 0.75f, 1.0f, 0.9f) : FLinearColor(0.22f, 0.48f, 0.68f, 0.5f),
            true, bOuterRing ? 1.5f : 1.0f);
    }

    const FLinearColor GridColour(0.22f, 0.48f, 0.68f, 0.55f);
    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(),
        { Center - FVector2D(Radius, 0.0f), Center + FVector2D(Radius, 0.0f) },
        ESlateDrawEffect::None, GridColour, true, 1.0f);
    FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(),
        { Center - FVector2D(0.0f, Radius), Center + FVector2D(0.0f, Radius) },
        ESlateDrawEffect::None, GridColour, true, 1.0f);

    const FVector LocalLocation = LocalCharacter->GetActorLocation();
    const float Yaw = PC->GetControlRotation().Yaw;
    const auto ProjectToRadar = [LocalLocation, Yaw, Radius, this](const FVector& WorldLocation, FVector2D& OutPoint)
    {
        FVector2D Delta(WorldLocation.X - LocalLocation.X, WorldLocation.Y - LocalLocation.Y);
        if (Delta.SizeSquared() > FMath::Square(RadarRange)) return false;
        Delta = Delta.GetRotated(-Yaw) * (Radius / RadarRange);
        OutPoint = Delta;
        return true;
    };
    const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 9);

    for (const TPair<TWeakObjectPtr<AWTBRCharacter>, FRadarContact>& Pair : Contacts)
    {
        const FRadarContact& Contact = Pair.Value;
        if (!Contact.Character.IsValid()) continue;

        const FLinearColor Colour = Contact.bFriendly
            ? FLinearColor(0.2f, 0.65f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.18f, 0.18f, 1.0f);

        TArray<FVector2D> TrailPoints;
        for (const FRadarTrailSample& Sample : Contact.Trail)
        {
            FVector2D RadarOffset;
            if (ProjectToRadar(Sample.Location, RadarOffset))
            {
                TrailPoints.Add(Center + FVector2D(RadarOffset.X, -RadarOffset.Y));
            }
        }
        if (TrailPoints.Num() > 1)
        {
            FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, AllottedGeometry.ToPaintGeometry(), TrailPoints,
                ESlateDrawEffect::None, Colour.CopyWithNewOpacity(0.45f), true, 1.5f);
        }

        FVector2D RadarOffset;
        if (!ProjectToRadar(Contact.Location, RadarOffset)) continue;
        const FVector2D MarkerCenter = Center + FVector2D(RadarOffset.X, -RadarOffset.Y);
        FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(MarkerCenter - FVector2D(3.0f, 3.0f), FVector2D(6.0f, 6.0f)),
            WhiteBrush, ESlateDrawEffect::None, Colour);

        const float VerticalDelta = Contact.Location.Z - LocalLocation.Z;
        if (FMath::Abs(VerticalDelta) >= VerticalLabelThreshold)
        {
            const FText HeightLabel = FText::FromString(VerticalDelta > 0.0f ? TEXT("HIGH") : TEXT("LOW"));
            FSlateDrawElement::MakeText(OutDrawElements, ++LayerId,
                AllottedGeometry.ToPaintGeometry(MarkerCenter + FVector2D(5.0f, -12.0f), FVector2D(28.0f, 12.0f)),
                HeightLabel, LabelFont, ESlateDrawEffect::None, Colour);
        }
    }

    FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
        AllottedGeometry.ToPaintGeometry(Center - FVector2D(4.0f, 4.0f), FVector2D(8.0f, 8.0f)),
        WhiteBrush, ESlateDrawEffect::None, FLinearColor::White);
    return LayerId;
}
