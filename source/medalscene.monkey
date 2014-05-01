Strict
Import vsat
Import menu
Import medalitem
Import back
Import game


Class MedalScene Extends VScene Implements VActionEventHandler
	
	Field normalBgColor:Color = New Color(Color.Navy)
	
'--------------------------------------------------------------------------
' * Init
'--------------------------------------------------------------------------	
	Method OnInit:Void()
		InitMenuTransition()
		InitBackgroundEffect()
		
		backFont = FontCache.GetFont(RealPath("font2"))
		descriptionFont = FontCache.GetFont(RealPath("font"))
		MedalItem.font = descriptionFont
		
		back = New BackButton
		back.SetFont(backFont)
		
		description = New MedalDescription
		description.SetFont(descriptionFont)
		description.Description = ""
		description.position.y = -description.size.y
		
		siteActive = ImageCache.GetImage(RealPath("siteActive.png"))
		siteNotActive = ImageCache.GetImage(RealPath("siteNotActive.png"))
		MidHandleImage(siteActive)
		MidHandleImage(siteNotActive)
		
		globalAlpha.Alpha = 0.0
		AddAction(New VFadeToAlphaAction(globalAlpha, 1.0, 0.7, LINEAR_TWEEN))
		
		InitMedals()
	End
	
	Method InitMenuTransition:Void()
		Local mainMenu:Object = Vsat.RestoreFromClipboard("MainMenu")
		If mainMenu
			Self.mainMenuObject = MainMenu(mainMenu)
			Local transition:= New MoveUpTransition(1.2)
			transition.SetScene(Self.mainMenuObject)
			Vsat.StartFadeIn(transition)
			
			backgroundColor.Set(mainMenuObject.backgroundColor)
			Local fadeColor:= New VFadeToColorAction(backgroundColor, normalBgColor, 0.5, LINEAR_TWEEN)
			AddAction(fadeColor)
		End
	End
	
	Method InitBackgroundEffect:Void()
		Local effect:Object = Vsat.RestoreFromClipboard("BgEffect")
		If effect
			backgroundEffect = ParticleBackground(effect)
		End
	End
	
	Method InitMedals:Void()
		medalItems = New CustomMedalItem[9]
		medalItems[0] = New CustomMedalItem("Normal-Dodge", "normal_dodge.png")
		medalItems[1] = New CustomMedalItem("Double-Dodge", "double_dodge.png")
		medalItems[2] = New CustomMedalItem("Triple-Dodge", "triple_dodge.png")
		medalItems[3] = New CustomMedalItem("Multi-Dodge", "multi_dodge.png")
		medalItems[4] = New CustomMedalItem("Close One", "close_one.png")
		medalItems[5] = New CustomMedalItem("Not Surprised", "not_surprised.png")
		medalItems[6] = New CustomMedalItem("Half-Dead", "half_dead.png")
		medalItems[7] = New CustomMedalItem("Scoreman", "scoreman.png")
		
		'Supporter
		If GameScene.IsUnlocked
			medalItems[medalItems.Length-1] = New CustomMedalItem("Supporter", "unlocked.png")
			medalItems[medalItems.Length-1].color.Set($e9f124)
		Else
			medalItems[medalItems.Length-1] = New CustomMedalItem("Supporter", "locked.png")
			medalItems[medalItems.Length-1].color.Set($c0c546)
		End
		
		
		Local hasWideScreen:Bool = Vsat.ScreenWidth / Vsat.ScreenHeight > 0.74
		If hasWideScreen 'iPad
			'Do something here
		Else 'iPhone
			Local x1:Float = Int(Vsat.ScreenWidth * 0.3)
			Local x2:Float = Int(Vsat.ScreenWidth * 0.7)
			Local y:Float

			For Local i:Int = 0 Until medalItems.Length
				If i Mod 6 = 0
					y = (Vsat.ScreenHeight - medalItems[0].Height * 3.25)/2
				End
				Local item:= medalItems[i]
				item.position.y = Int(y)
				If i Mod 2 = 0
					item.position.x = x1 + (i/6 * Vsat.ScreenWidth)
				Else
					item.position.x = x2 + (i/6 * Vsat.ScreenWidth)
					y += item.Height * 1.2
				End
			Next
			
			sites = medalItems.Length / 6 + 1
		End
		
	End
	
	Method AddAction:Void(action:VAction)
		action.AddToList(actions)
		action.SetListener(Self)
	End
	
	
'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------	
	Method OnUpdate:Void(dt:Float)
		VAction.UpdateList(actions, dt)
		If backgroundEffect Then backgroundEffect.Update(dt)
		UpdateSites()
		UpdateCursor()
	End
	
	Method UpdateCursor:Void()
		If TouchDown()
			OnMouseDown()
			lastTouchDown = True
		Else
			If lastTouchDown
				OnMouseUp()
			End
			lastTouchDown = False
		End
	End
	
	Method UpdateSites:Void()
		If TouchDown() Return
		If targetPosX <> -99999
			Local lerp:Float = 0.1
			currentPosX += (targetPosX - currentPosX) * lerp
			If Abs(targetPosX - currentPosX) < 0.01
				currentPosX = targetPosX
				targetPosX = -99999
			End
		End
	End
	

'--------------------------------------------------------------------------
' * Render
'--------------------------------------------------------------------------
	Method OnRender:Void()
		If backgroundEffect
			backgroundEffect.Render()
		End
		
		If Vsat.IsChangingScenes()
			If MoveDownTransition(Vsat.transition)
				Local transition:= MoveDownTransition(Vsat.transition)
				If transition.startPoint < 0
					Translate(0, Vsat.ScreenHeight * Vsat.transition.Progress * 0.2)
				End
			End
		End
		
		RenderDescription()
		Translate(0, description.position.y + description.size.y)
		
		RenderBackButton()
		RenderMedals()
		RenderSites()
	End
	
	Method RenderBackButton:Void()
		back.Render()
	End
	
	Method RenderDescription:Void()
		description.Render()
	End
	
	Method RenderMedals:Void()
		PushMatrix()
		Translate(-currentPosX, 0)
		For Local i:Int = 0 Until medalItems.Length
			If i = medalItems.Length-1
				If GameScene.IsUnlocked
					medalItems[i].color.Alpha = Min(0.85 + Sin(Vsat.Seconds*150)*0.3, 1.0)
				Else
					medalItems[i].color.Alpha = Min(0.7 + Sin(Vsat.Seconds*150)*0.5, 1.0)
				End
			End
			medalItems[i].Render()
		Next
		PopMatrix()
	End
	
	Method RenderSites:Void()
		Color.White.Use()
		
		Local siteRadius:Float = siteActive.Width()/2 * 0.8
		Local x:Float = Vsat.ScreenWidth2 + siteRadius
		If sites Mod 2 = 0
			x -= (siteRadius * 3 * sites/2) / 2
		Else
			x -= (siteRadius * 3 * sites/2)
		End
		
		For Local i:Int = 1 To sites
			If i = currentSite
				DrawImage(siteActive, x, Vsat.ScreenHeight - siteRadius * 3)
				' DrawCircle(x, Vsat.ScreenHeight - siteRadius * 4, siteRadius)
			Else
				DrawImage(siteNotActive, x, Vsat.ScreenHeight - siteRadius * 3)
				' DrawCircleOutline(x, Vsat.ScreenHeight - siteRadius * 4, siteRadius, siteRadius * 2)
			End
			x += siteRadius * 3
		Next
	End
	
	Method ClearScreen:Void()
		ClearScreenWithColor(backgroundColor)
	End
	

'--------------------------------------------------------------------------
' * Events
'--------------------------------------------------------------------------
	Method OnMouseUp:Void()
		Local cursor:Vec2 = New Vec2(TouchX(), RealY(TouchY()))
		If back.isDown
			back.isDown = False
			If back.WasTouched(cursor)
				OnBackClicked()
			End
			Return
		End
		
		touchEndX = cursor.x + (Vsat.ScreenWidth * (currentSite-1))
		If touchEndX = touchStartX
			OnItemClicked()
			Return
		End
		
		touchTime = Vsat.Seconds - touchStartTime
		Local distance:Float = touchEndX - touchStartX
		Local speed:Float = distance / touchTime
		If Abs(speed) > Vsat.ScreenWidth2
			If speed < 0
				currentSite += 1
				currentSite = Min(currentSite, sites)
			ElseIf speed > 0
				currentSite -= 1
				currentSite = Max(currentSite, 1)
			End
			
			Local expectedSpeed:Float = 400.0
			Local time:Float = Max(0.5 * expectedSpeed/Abs(speed), 0.3)
			Local action:= New VVec2ToAction(description.position, 0, -description.size.y, time, EASE_OUT_CIRC)
			AddAction(action)
		End
		targetPosX = Vsat.ScreenWidth * (currentSite-1)
	End
	
	Method OnMouseDown:Void()
		Local cursor:Vec2 = New Vec2(TouchX(), RealY(TouchY()))
		If back.WasTouched(cursor)
			back.isDown = True
			Return
		End
		If back.isDown Return
		
		If lastTouchDown = False
			touchStartX = cursor.x + (Vsat.ScreenWidth * (currentSite-1))
			touchStartTime = Vsat.Seconds
		Else
			currentPosX = touchStartX - cursor.x
		End
	End
	
	Method OnBackClicked:Void()
		If Vsat.transition Return
		AddAction(New VFadeToAlphaAction(globalAlpha, 0.0, 0.5, EASE_OUT_EXPO))
		mainMenuObject.shouldClearScreen = True
		Local transition:= New MoveDownTransition(0.7)
		transition.startPoint = -Vsat.ScreenHeight
		transition.SetScene(mainMenuObject)
		Vsat.ChangeSceneWithTransition(mainMenuObject, transition)
		
		Local fadeColor:= New VFadeToColorAction(backgroundColor, mainMenuObject.backgroundColor, 0.5, LINEAR_TWEEN)
		AddAction(fadeColor)
	End
	
	Method OnItemClicked:Void()
		Local x:Float = TouchX() + (Vsat.ScreenWidth * (currentSite-1))
		Local y:Float = RealY(TouchY())
		Local cursor:= New Vec2(x, y)
		Local startIndex:Int = (currentSite-1) * 6
		Local endIndex:Int = Min(startIndex+6, medalItems.Length)
		
		For Local i:Int = startIndex Until endIndex
			Local item:= medalItems[i]
			If item.WasTouched(cursor)
				If item.Description = "" And GameScene.IsUnlocked = False
					GoToSupporter()
				Else
					ItemWasClicked(item)
				End
				Return
			End
		Next
		
		'Clicked in empty space
		Local action:= New VVec2ToAction(description.position, 0, -description.size.y, 0.3, EASE_OUT_CIRC)
		AddAction(action)
	End
	
	Method ItemWasClicked:Void(item:CustomMedalItem)
		If description.position.y = 0.0
			If description.Description = item.Description
				Local action:= New VVec2ToAction(description.position, 0, -description.size.y, 0.2, EASE_OUT_CIRC)
				AddAction(action)
				Return
			End
			Local fadeOut:= New VFadeToAlphaAction(description.color, 0.5, 0.1, LINEAR_TWEEN)
			Local fadeIn:= New VFadeToAlphaAction(description.color, 1.0, 0.1, LINEAR_TWEEN)
			Local group:= New VActionSequence
			group.AddAction(fadeOut)
			group.AddAction(fadeIn)
			AddAction(group)
		Else
			Local action:= New VVec2ToAction(description.position, 0, 0, 0.2, EASE_OUT_CIRC)
			AddAction(action)
		End
		description.Description = item.Description
	End
	
	Method GoToSupporter:Void()
		If Vsat.transition And VFadeInLinear(Vsat.transition) = Null
			Return
		End
		Vsat.SaveToClipboard(mainMenuObject, "MainMenu")
		mainMenuObject.shouldClearScreen = True
		Local scene:= New BuySupporterMedalScene
		Vsat.ChangeScene(scene)
	End
	
	Method OnActionEvent:Void(id:Int, action:VAction)
		If id = VAction.FINISHED
			
		End
	End
	

'--------------------------------------------------------------------------
' * Helpers
'--------------------------------------------------------------------------
	Method RealY:Float(value:Float)
		Return value -(description.position.y + description.size.y)
	End
	
	
	
'--------------------------------------------------------------------------
' * Private
'--------------------------------------------------------------------------
	Private
	Field mainMenuObject:MainMenu
	Field backgroundEffect:ParticleBackground
	Field backgroundColor:Color = New Color
	
	Field backFont:AngelFont
	Field descriptionFont:AngelFont
	Field description:MedalDescription
	
	Field back:BackButton
	
	Field lastTouchDown:Bool
	
	Field actions:List<VAction> = New List<VAction>
	
	Field medalItems:CustomMedalItem[]
	Field sites:Int
	Field currentSite:Int = 1
	Field currentPosX:Float
	Field targetPosX:Float = -99999
	Field touchStartX:Float
	Field touchEndX:Float
	Field touchStartTime:Float
	Field touchTime:Float
	
	Field siteActive:Image
	Field siteNotActive:Image
End





Private
Class CustomMedalItem Extends MedalItem
	Method New(name:String, fileName:String)
		Super.New(name, fileName)
		color.Set(Color.White)
	End
	
	Method Draw:Void()
		SetAlpha(color.Alpha * globalAlpha.Alpha)
		Super.Draw()
	End
	
	Method WasTouched:Bool(cursor:Vec2)
		Return PointInRect(cursor.x, cursor.y, position.x-Width/2, position.y, Width, Height)
	End
	
End


Class MedalDescription Extends VRect
	
	Method New()
		Super.New(0, 0, Vsat.ScreenWidth, Vsat.ScreenHeight * 0.06)
		color.Set(Color.White)
		descriptionLabel = New VLabel("description")
		descriptionLabel.position.Set(Vsat.ScreenWidth2, 0)
		descriptionLabel.alignHorizontal = True
	End
	
	Method SetFont:Void(font:AngelFont)
		descriptionLabel.SetFont(font)
		size.y = font.height * 0.8 * 1.5
	End
	
	Method Description:Void(text:String) Property
		descriptionLabel.Text = text
		If descriptionLabel.size.x > Vsat.ScreenWidth * 0.9
			factor = (Vsat.ScreenWidth * 0.9) / descriptionLabel.size.x
		Else
			factor = 0.8
		End
	End
	
	Method Description:String() Property
		Return descriptionLabel.Text
	End
	
	Method Draw:Void()
		SetAlpha(0.1 * globalAlpha.Alpha * color.Alpha)
		Super.Draw()
		PushMatrix()
			TranslateV(descriptionLabel.position)
			ScaleAt(0, descriptionLabel.size.y/2, scale.x * factor, scale.y * factor)
			SetAlpha(0.9 * globalAlpha.Alpha * color.Alpha)
			Color.White.UseWithoutAlpha()
			descriptionLabel.Draw()
		PopMatrix()
	End
	
	Private
	Field descriptionLabel:VLabel
	Field factor:Float
End







