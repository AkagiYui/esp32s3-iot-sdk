import { mount, hydrate } from 'svelte'
import './app.css'
import App from './App.svelte'

const target = document.getElementById('app')!
const shouldHydrate = target.dataset.ssgHydrate !== undefined

const app = shouldHydrate
  ? hydrate(App, { target, recover: true })
  : mount(App, { target })

export default app
