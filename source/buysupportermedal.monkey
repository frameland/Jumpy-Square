Strict
Import vsat
Import game
Import extra
Import supportermedal
Import medals
Import save


Class BuySupporterMedalScene Extends VScene

'--------------------------------------------------------------------------
' * Init
'--------------------------------------------------------------------------	
	Method OnInit:Void()
		Self.shouldClearScreen = False
		
		medal = New SupporterMedal
		medal.InitUnlocked()
		medal.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.35)
		
		font = FontCache.GetFont(RealPath("font"))
		
		If GameScene.IsUnlocked
			descriptionText = "Thank you for purchasing the Supporter Medal."
			descriptionText += "~nStay tuned for supporter exclusive content~nupdates in the near future."
		Else
			descriptionText = "Add the Supporter Medal to your collection."
			descriptionText += "~nIt will also remove all ads from the game."
		End
		renderedText = descriptionText.Split("~n")
		
		InitMenuChoices()
		InitMenu()
		
		Local transition:= New FadeInTransition(0.2)
		Vsat.StartFadeIn(transition)
	End
	
	Method InitMenu:Void()
		Local mainMenu:Object = Vsat.RestoreFromClipboard("MainMenu")
		If mainMenu
			Self.mainMenuObject = MainMenu(mainMenu)
		End
		AssertWithException(mainMenu, "Couldnt find MainMenu in Vsat-Clipboard")
	End
	
	Method InitMenuChoices:Void()
		Local price:String = "0.99"
		buy = New MenuItem("Buy it (" + price + ")")
		buy.alignHorizontal = AngelFont.ALIGN_RIGHT
		buy.SetFont(RealPath("font"))
		buy.position.x = Vsat.ScreenWidth*0.75
		buy.position.y = medal.position.y + medal.Height() * 0.75 + font.height * (renderedText.Length+0.4)
		buy.SetScale(0.8)
		buy.color.Set(Color.Orange)
		
		cancel = New MenuItem("No thanks")
		cancel.SetFont(RealPath("font"))
		cancel.position.x = Vsat.ScreenWidth*0.25
		cancel.position.y = buy.position.y
		cancel.SetScale(0.8)
		cancel.color.Set(Color.Orange)
		cancel.color.Alpha = 0.6
		
		If GameScene.IsUnlocked
			cancel.Text = "Go back"
			cancel.alignHorizontal = AngelFont.ALIGN_CENTER
			cancel.position.x = Vsat.ScreenWidth2
			cancel.color.Alpha = 1.0
		End
	End
	

'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------
	Method OnUpdate:Void(dt:Float)
		mainMenuObject.UpdateParticles(dt)
		UpdateCursor()
		medal.Update(dt)
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
	

'--------------------------------------------------------------------------
' * Render
'--------------------------------------------------------------------------
	Method OnRender:Void()
		mainMenuObject.ClearScreen()
		mainMenuObject.RenderParticles()
		mainMenuObject.OnRender()
		RenderBackground()
		
		If Vsat.IsChangingScenes()
			Local scaleAll:Float
			If FadeInTransition(Vsat.transition)
				scaleAll = 2.0 - Vsat.transition.Progress
				globalAlpha.Alpha = Vsat.transition.Progress
			ElseIf FadeOutTransition(Vsat.transition)
				scaleAll = 1.0 + Tweening(EASE_OUT_EXPO, Vsat.transition.Progress, 0.0, 1.0, Vsat.transition.Duration)
				globalAlpha.Alpha = 2.0 - scaleAll
			End
			ScaleAt(Vsat.ScreenWidth2, Vsat.ScreenHeight2, scaleAll, scaleAll)
		End
		
		RenderMedal()
		RenderDescription()
		RenderChoices()
	End
	
	Method RenderBackground:Void()
		If Not Vsat.IsChangingScenes()
			Color.NewBlack.UseWithoutAlpha()
			SetAlpha(0.95)
			DrawRect(0, 0, Vsat.ScreenWidth, Vsat.ScreenHeight)
		End
	End
	
	Method RenderMedal:Void()
		medal.color.Alpha = globalAlpha.Alpha
		medal.Render()
	End
	
	Method RenderDescription:Void()
		ResetColor()
		SetAlpha(globalAlpha.Alpha)
		PushMatrix()
			Translate(Vsat.ScreenWidth2, medal.position.y + medal.Height() * 0.75)
			Scale(0.8, 0.8)
			For Local i:Int = 0 Until renderedText.Length
				font.DrawText(renderedText[i], 0, i*font.height, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_TOP)
			Next
		PopMatrix()
	End
	
	Method RenderChoices:Void()
		If GameScene.IsUnlocked = False
			buy.Render()
		End
		cancel.Render()
	End


'--------------------------------------------------------------------------
' * Events
'--------------------------------------------------------------------------
	Method OnMouseUp:Void()
		Local cursor:Vec2 = New Vec2(TouchX(), TouchY())
		If buy.isDown And GameScene.IsUnlocked = False
			buy.isDown = False
			If buy.WasTouched(cursor)
				OnBuy()
			End
		ElseIf cancel.isDown
			cancel.isDown = False
			If cancel.WasTouched(cursor)
				OnCancel()
			End
		End
	End
	
	Method OnMouseDown:Void()
		Local cursor:Vec2 = New Vec2(TouchX(), TouchY())
		If buy.WasTouched(cursor) And GameScene.IsUnlocked = False
			buy.isDown = True
		ElseIf cancel.WasTouched(cursor)
			cancel.isDown = True
		End
	End
	
	Method OnCancel:Void()
		If Vsat.transition Return
		Local transition:= New FadeOutTransition(0.6)
		Vsat.ChangeSceneWithTransition(mainMenuObject, transition)
		
		medal.UnlockTime = -1.0 'ugly fix for particles (they have no alpha)
	End
	
	Method OnBuy:Void()
		If GameScene.IsUnlocked
			OnCancel()
			Return
		End
		
		Medals.Supporter += 1
		GameScene.IsUnlocked = True
		mainMenuObject.justGotSupporterMedal = True
		OnCancel()
		SaveGame()
	End
	
	
	Private
	Field mainMenuObject:MainMenu
	Field medal:SupporterMedal
	
	Field font:AngelFont
	Field descriptionText:String
	Field renderedText:String[]
	
	Field buy:MenuItem
	Field cancel:MenuItem
	
	Field lastTouchDown:Bool
	
End



Private
Class FadeInTransition Extends VTransition
	
	Field startPoint:Float = 0
	
	Method New()
		Super.New()
	End
	
	Method New(duration:Float)
		Super.New(duration)
	End
	
	Method Render:Void()
		PushMatrix()
		ResetMatrix()
		ResetBlend()
		Local progress:Float = Tweening(LINEAR_TWEEN, Time, 0.0, 1.0, Duration)
		SetAlpha(progress * 0.95)
		Color.NewBlack.UseWithoutAlpha()
		DrawRect(0, 0, Vsat.ScreenWidth, Vsat.ScreenHeight)
		PopMatrix()
	End

End

Class FadeOutTransition Extends VTransition
	
	Field startPoint:Float = 0
	
	Method New()
		Super.New()
	End
	
	Method New(duration:Float)
		Super.New(duration)
	End
	
	Method Render:Void()
		PushMatrix()
		ResetMatrix()
		ResetBlend()
		Local progress:Float = Tweening(LINEAR_TWEEN, Time, 0.0, 1.0, Duration)
		SetAlpha(0.95 - progress * 0.95)
		Color.NewBlack.UseWithoutAlpha()
		DrawRect(0, 0, Vsat.ScreenWidth, Vsat.ScreenHeight)
		PopMatrix()
	End

End


Class MenuItem Extends VLabel
	
	Field isDown:Bool
	Field downColor:Color = New Color(1.0, 1.0, 1.0, 0.8)
	
	Method New(withText:String)
		Super.New(withText)
		color.Set(Color.White)
	End
	
	Method Draw:Void()
		If isDown
			downColor.Use()
			SetAlpha(downColor.Alpha * globalAlpha.Alpha)
		Else
			SetAlpha(color.Alpha * globalAlpha.Alpha)
		End
		Super.Draw()
	End
	
	Method WasTouched:Bool(cursor:Vec2)
		If alignHorizontal = AngelFont.ALIGN_LEFT
			Return PointInRect(cursor.x, cursor.y, position.x, position.y, size.x, size.y)
		ElseIf alignHorizontal = AngelFont.ALIGN_RIGHT
			Return PointInRect(cursor.x, cursor.y, position.x-size.x, position.y, size.x, size.y)
		ElseIf alignHorizontal = AngelFont.ALIGN_CENTER
			Return PointInRect(cursor.x, cursor.y, position.x-size.x/2, position.y, size.x, size.y)
		End
		Return False
	End
	
End




