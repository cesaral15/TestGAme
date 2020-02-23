// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "surivivorCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include <Runtime\Engine\Public\Net\UnrealNetwork.h>


//////////////////////////////////////////////////////////////////////////
// AsurivivorCharacter

AsurivivorCharacter::AsurivivorCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;
	bDashing = false;
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	//GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

void AsurivivorCharacter::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate to everyone
	DOREPLIFETIME(AsurivivorCharacter, bDashing);
}

//////////////////////////////////////////////////////////////////////////
// Input

void AsurivivorCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("DashRight", IE_DoubleClick, this, &AsurivivorCharacter::DashRight);
	/*PlayerInputComponent->BindAction("DashLeft", IE_DoubleClick, this, &AsurivivorCharacter::DashLeft);
	PlayerInputComponent->BindAction("DashForward", IE_DoubleClick, this, &AsurivivorCharacter::DashLeft);
	PlayerInputComponent->BindAction("DashBackward", IE_DoubleClick, this, &AsurivivorCharacter::DashLeft);*/
	
	PlayerInputComponent->BindAction<DirectionDelagate>("DashRight", IE_DoubleClick, this, &AsurivivorCharacter::Dash, 0);
	PlayerInputComponent->BindAction<DirectionDelagate>("DashLeft", IE_DoubleClick, this, &AsurivivorCharacter::Dash, 1);
	PlayerInputComponent->BindAction<DirectionDelagate>("DashForward", IE_DoubleClick, this, &AsurivivorCharacter::Dash, 2);
	PlayerInputComponent->BindAction<DirectionDelagate>("DashBackward", IE_DoubleClick, this, &AsurivivorCharacter::Dash, 3);



	PlayerInputComponent->BindAxis("MoveForward", this, &AsurivivorCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AsurivivorCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AsurivivorCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AsurivivorCharacter::LookUpAtRate);


	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AsurivivorCharacter::OnResetVR);
}


bool AsurivivorCharacter::GetDash()
{
	return bDashing;
}

void AsurivivorCharacter::StopDash()
{
	bDashing = false;
}

void AsurivivorCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AsurivivorCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AsurivivorCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AsurivivorCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	//AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AsurivivorCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	//AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AsurivivorCharacter::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		//GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Yellow, FString::SanitizeFloat(Value));
		AddMovementInput(Direction, Value);
	}
}

void AsurivivorCharacter::MoveRight(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		//GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Yellow, FString::SanitizeFloat(Value));
		AddMovementInput(Direction, Value);
	}
}

void AsurivivorCharacter::Dash(int movement)
{
	if ((Controller != NULL))
	{
		if (Role < ROLE_Authority)
		{
			ServerDash(movement);
		}
	}
}


bool AsurivivorCharacter::ServerDash_Validate(int movement)
{
	
	return true;
}

void AsurivivorCharacter::ServerDash_Implementation(int movement)
{
	if (Role == ROLE_Authority)
	{
		//0 -> right
		//1 -> left
		//2 -> forward
		//3 -> backward
		
		bDashing = true;
		if (movement == 0)
		{
			LaunchCharacter(FVector(GetActorRightVector().X ,GetActorRightVector().Y, 0).GetSafeNormal() * 1000.f, true, true);
		}
		else if (movement == 1)
		{
			LaunchCharacter(FVector(-(GetActorRightVector().X), -(GetActorRightVector().Y), 0).GetSafeNormal() * 1000.f, true, true);
		}
		else if (movement == 2)
		{
			LaunchCharacter(FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0).GetSafeNormal() * 1000.f, true, true);
		}
		else
		{
			LaunchCharacter(FVector(-(GetActorForwardVector().X), -(GetActorForwardVector().Y), 0).GetSafeNormal() * 1000.f, true, true);
		}
		
		if (bDashing)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Yellow, "true");
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Yellow, "false");
		}
		GetWorld()->GetTimerManager().SetTimer(DashHandle, this, &AsurivivorCharacter::StopDash, 0.5f, false);
	}
}