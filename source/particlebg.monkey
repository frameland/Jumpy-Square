Strict
Import vsat
Import extra


Class ParticleBackground
	
	Field emitter:CircleParticleEmitter
	Field lastFrame:Int
	Field baseUnit:Float
	
	Method New()
		baseUnit = Vsat.ScreenWidth2
		emitter = New CircleParticleEmitter
		emitter.InitWithSize(120)
		emitter.particleLifeSpan = 12
		emitter.particleLifeSpanVariance = 5.0
		emitter.emissionRate = 10
		
		emitter.size.Set(1, 1)
		emitter.endSize.Set(24, 24)
		If IsHD() Then emitter.endSize.Mul(2)

		emitter.startColor.Alpha = 0.1
		emitter.endColor.Alpha = 0.0
		
		emitter.speed = 85
		emitter.speedVariance = 35
		If IsHD()
			emitter.speed *= 2
			emitter.speedVariance *= 2
		End
		emitter.positionVariance.Set(baseUnit, Vsat.ScreenHeight2)
		
		SetNormal()
		
		InitGradient()
	End
	
	Method InitGradient:Void()
		topGradient = New Sprite("gfx/top_gradient.png")
		topGradient.SetHandle(0, 0)
		topGradient.SetScale(Vsat.ScreenWidth/topGradient.Width)
		topGradient.color.Set($6effe1)
	End
	
	Method SetNormal:Void()
		emitter.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.8)
		emitter.emissionAngle = -90
		emitter.Start()
		emitter.FastForward(10, 0.016666)
	End
	
	Method SetPlay:Void()
		emitter.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.2)
		emitter.emissionAngle = 90
		emitter.Start()
		emitter.FastForward(10, 0.016666)
	End
	
	Method Update:Void(dt:Float)
		If lastFrame = Vsat.Frame
			Return
		End
		lastFrame = Vsat.Frame
		emitter.Update(dt)
		
		topGradient.Alpha = 0.9 + Sin((Vsat.Seconds * 100) Mod 360) * 0.1
	End
	
	Method Render:Void()
		topGradient.Render()
		emitter.Render()
	End
	
	Private
	Field topGradient:Sprite
	
End




